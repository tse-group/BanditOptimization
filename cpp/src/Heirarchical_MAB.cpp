//
// Created by Govinda Kamath on 2/12/18.
//

#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <thread>
#include <numeric>
#include <map>
#include "Points.h"
#include "Arms.h"
#include "../utilities/INIReader.h"
#include "Knn.h"


int main()
{
//    std::string nameConfig = argv[1];

//     For debugging mode in CLion
    std::string nameConfig = "/Users/vivekkumarbagaria/Code/combinatorial_MAB/nominal.ini";

    // Parameters
    INIReader reader(nameConfig);
    if (reader.ParseError() < 0) {
        std::cout << "Can't load "<< nameConfig << std::endl;
        return 1;
    }

    // Loading Hyper parameters and data sizes
    std::string directoryPath = reader.Get("path", "directory", "");
    std::string saveFilePath =reader.Get("path", "saveFilePath", "test.output");
    std::string fileSuffix = reader.Get("path", "suffix", "");
    unsigned numberOfInitialPulls = (unsigned) reader.GetInteger("UCB", "numberOfInitialPulls_knn", 100);
    unsigned sampleSize = (unsigned) reader.GetInteger("UCB", "sampleSize", 32);
    float delta = (float) reader.GetReal("UCB", "delta", 0.1);


    std::vector<std::string>  pathsToImages;
    std::vector<SquaredEuclideanPoint> pointsVec;
    std::vector<std::shared_ptr<SquaredEuclideanPoint> > sharedPtrPointsVec;
    std::vector<GroupPoint<SquaredEuclideanPoint> > groupPoints;
    std::vector<std::string> groupPointsNames; // Only for debugging purpose: Stores the neighbhours
    std::vector<ArmHeirarchical<SquaredEuclideanPoint> > armsVec;
    // Stores map from group id to arm id. Used to removes "irrelevant" arms
    std::unordered_map<std::pair<unsigned long, unsigned long >, unsigned long, utils::pair_hash> groupIDtoArmID;

    //Maintains the valid group points currently.
    //Used while creating new arms
    std::unordered_set<unsigned long > groupPointsInPlay;


    utils::getPathToFile(pathsToImages, directoryPath, fileSuffix); // Loads the filepaths into an array
    utils::vectorsToPoints(pointsVec, pathsToImages); //Loads the images from the locations in above array to pointsVec

    // Step 1: Initialize all the leaves
    unsigned long armID = 0;
    unsigned long n = 1000; // pointsVec.size();
    unsigned long d = pointsVec[0].getVecSize();
    unsigned long maxGroupPointId (0);
    std::cout << "Running Hierarchical clustering for " << n << " points" << std::endl;

    groupIDtoArmID.reserve(n*n);
    groupPoints.reserve(n*n);

    // Pushing null objects
    for(unsigned long i(0); i< n*n; i++){
        groupPoints.push_back(GroupPoint<SquaredEuclideanPoint> ()); //Todo: Bad Code
        groupPointsNames.push_back("");
    }

    for (unsigned long i(0); i < n; i++) {
        sharedPtrPointsVec.push_back(std::make_shared<SquaredEuclideanPoint>(pointsVec[i]));
        std::vector<std::shared_ptr<SquaredEuclideanPoint> > groupPointTmp1;
        groupPointTmp1.push_back(sharedPtrPointsVec[i]);
        GroupPoint<SquaredEuclideanPoint> gpTmp1(groupPointTmp1, d, maxGroupPointId);
        groupPoints[i] = gpTmp1;
        groupPointsNames[i] =  std::to_string(i);
        groupPointsInPlay.insert(maxGroupPointId);
        maxGroupPointId++;

    }

    for (unsigned long i(0); i < n; i++) {
        for (unsigned long j(0); j < i; j++) {
            ArmHeirarchical<SquaredEuclideanPoint> tmpArm(armID, groupPoints[i], groupPoints[j]);
            groupIDtoArmID[std::make_pair(i,j)] = armID;
            armsVec.push_back(tmpArm);
            armID++;
        }
    }

    UCBDynamic<ArmHeirarchical<SquaredEuclideanPoint> > UCB1(armsVec, delta, 1, 0, sampleSize);

#define UCB
#ifdef UCB
    std::cout << "Running MAB with sample size " << sampleSize << std::endl;
    std::chrono::system_clock::time_point t1 = std::chrono::system_clock::now();
    UCB1.initialise((unsigned )numberOfInitialPulls);
    std::chrono::system_clock::time_point t2 = std::chrono::system_clock::now();
    long long int Tt = std::chrono::duration_cast<std::chrono::milliseconds>
            (t2-t1).count();
    std::cout << "Init Time = " << Tt << "(ms)" << std::endl;

#else
    std::cout << "Running Brute" << std::endl;
    UCB1.armsKeepFromArmsContainerBrute();
#endif

    std::chrono::system_clock::time_point timeStartStart = std::chrono::system_clock::now();
    unsigned long prevGlobalNumberPulls = UCB1.globalNumberOfPulls;

    // Step 2: Run clustering
    for(unsigned long i(0); i < n-2; i++){
        std::chrono::system_clock::time_point timeStart = std::chrono::system_clock::now();
        //Find the best group points to join
#ifdef UCB
        UCB1.runUCB(n*n*d);
        ArmHeirarchical<SquaredEuclideanPoint> bestArm = UCB1.topKArms.back();
#else
        ArmHeirarchical<SquaredEuclideanPoint> bestArm = UCB1.bruteBestArm();
#endif

//        std::cout << "Step " << i  << " Arm ID " << bestArm.id  << " Avg Pulls" << UCB1.globalNumberOfPulls/(n*n*.5);
        auto left  = bestArm.leftGroupID;
        auto right = bestArm.rightGroupID;


//        std::cout <<" Left " << groupPointsNames[left]<< " Right " << groupPointsNames[right];
        //Removing arms corresponding to the left and right groups of the best arm
        for(unsigned long index(0); index < maxGroupPointId ; index++){
            if(groupIDtoArmID.find(std::make_pair(left, index)) != groupIDtoArmID.end()){
                UCB1.markForRemoval(groupIDtoArmID[std::make_pair(left, index)]);
            }
            if(groupIDtoArmID.find(std::make_pair(index, left)) != groupIDtoArmID.end()){
                UCB1.markForRemoval(groupIDtoArmID[std::make_pair(index, left)]);
            }
            if(groupIDtoArmID.find(std::make_pair(right, index)) != groupIDtoArmID.end()){
                UCB1.markForRemoval(groupIDtoArmID[std::make_pair(right, index)]);
            }
            if(groupIDtoArmID.find(std::make_pair(index, right)) != groupIDtoArmID.end()){
                UCB1.markForRemoval(groupIDtoArmID[std::make_pair(index, right)]);
            }
        }

        // Removing left and right arm from the groups (nodes)
        groupPointsInPlay.erase(left);
        groupPointsInPlay.erase(right);

        //Creating a new group point by combining left and right points
        //Create the vector of pointers to the points in the new group point
        std::vector<std::shared_ptr<SquaredEuclideanPoint > > newGroupPoint;
        newGroupPoint = bestArm.leftGroupPoint->groupPoint;
        newGroupPoint.insert(newGroupPoint.end(), bestArm.rightGroupPoint->groupPoint.begin(),
                             bestArm.rightGroupPoint->groupPoint.end());


        //Create a new group point
        GroupPoint<SquaredEuclideanPoint> gpTmp(newGroupPoint, d, maxGroupPointId);
        groupPoints[maxGroupPointId] = gpTmp;
        groupPointsNames[maxGroupPointId] = groupPointsNames[left]+" "+groupPointsNames[right];

        unsigned  long idOfInsertedPoint = maxGroupPointId;
        maxGroupPointId++;

        //Add and initialise best arm
        for (const auto& pointID: groupPointsInPlay) {
            ArmHeirarchical<SquaredEuclideanPoint> tmpArm(armID, groupPoints[idOfInsertedPoint], groupPoints[pointID]);
            groupIDtoArmID[std::make_pair(idOfInsertedPoint, pointID)] = armID;

            unsigned long leftArmRemovedId;
            unsigned long rightArmRemovedId;

            if(groupIDtoArmID.find(std::make_pair(left, pointID)) != groupIDtoArmID.end()){
                leftArmRemovedId = groupIDtoArmID[std::make_pair(left, pointID)];
            }
            else if (groupIDtoArmID.find(std::make_pair(pointID, left)) != groupIDtoArmID.end()){
                leftArmRemovedId = groupIDtoArmID[std::make_pair(pointID, left)];
            }
            else{
                throw std::runtime_error("[Unexpected behaviour]: Marked left arm's id not found.");
            }

            if(groupIDtoArmID.find(std::make_pair(right, pointID)) != groupIDtoArmID.end()){
                rightArmRemovedId = groupIDtoArmID[std::make_pair(right, pointID)];
            }
            else if (groupIDtoArmID.find(std::make_pair(pointID, right)) != groupIDtoArmID.end()){
                rightArmRemovedId = groupIDtoArmID[std::make_pair(pointID, right)];
            }
            else{
                throw std::runtime_error("[Unexpected behaviour]: Marked right arm's id not found.");
            }
            unsigned long numArmPulls(0);
            float armSumOfPulls(0.0);
            float armSumOfSquaresOfPulls(0.0);
            float trueMeanValue(0.0);
            if(UCB1.armStates.find(leftArmRemovedId) != UCB1.armStates.end()){
                numArmPulls += UCB1.armStates[leftArmRemovedId].numberOfPulls;
                armSumOfPulls += UCB1.armStates[leftArmRemovedId].sumOfPulls;
                armSumOfSquaresOfPulls += UCB1.armStates[leftArmRemovedId].sumOfSquaresOfPulls;
                trueMeanValue += UCB1.armStates[leftArmRemovedId].trueMeanValue;
            }
            else{
                throw std::runtime_error("[Unexpected behaviour]: Marked left arm's state not found.");
            }
            if(UCB1.armStates.find(rightArmRemovedId) != UCB1.armStates.end()){
                numArmPulls += UCB1.armStates[rightArmRemovedId].numberOfPulls;
                armSumOfPulls += UCB1.armStates[rightArmRemovedId].sumOfPulls;
                armSumOfSquaresOfPulls += UCB1.armStates[rightArmRemovedId].sumOfSquaresOfPulls;
                trueMeanValue += UCB1.armStates[rightArmRemovedId].trueMeanValue;
            }
            else{
                throw std::runtime_error("[Unexpected behaviour]: Marked right arm's state not found.");
            }
            tmpArm.warmInitialise(numArmPulls, armSumOfPulls, armSumOfSquaresOfPulls, trueMeanValue/2);
#ifdef UCB
            UCB1.initialiseAndAddNewArm(tmpArm, 0); //0 initial pulls
#else
            tmpArm.lowerConfidenceBound = tmpArm.trueMeanValue;
            tmpArm.upperConfidenceBound = tmpArm.trueMeanValue;
            tmpArm.estimateOfMean = tmpArm.trueMeanValue;
            UCB1.initialiseAndAddNewArmBrute(tmpArm, 0);
#endif
            armID++;
        }

        //Adding inserted point to points in play
        groupPointsInPlay.insert(idOfInsertedPoint);
        std::chrono::system_clock::time_point timeEnd = std::chrono::system_clock::now();
        long long int trueMeanTime = std::chrono::duration_cast<std::chrono::milliseconds>
                (timeEnd-timeStart).count();

        std::chrono::system_clock::time_point timeEndEnd = std::chrono::system_clock::now();
        long long int totalTime = std::chrono::duration_cast<std::chrono::milliseconds>
                (timeEndEnd-timeStartStart).count();

        float localVar = bestArm.estimateOfSecondMoment - std::pow(bestArm.estimateOfMean,2);
        std::cout   << "Step " << i
                << "\tEst. = " << bestArm.estimateOfMean
                << "\tId. = " << bestArm.id
                << "\tTime = " << trueMeanTime
                    << "\tT-Time = " << totalTime
//                      << "\tGlobal number of Pulls = " << UCB1.globalNumberOfPulls
                    << "\tNew no. of Puls = " << UCB1.globalNumberOfPulls - prevGlobalNumberPulls
//                    << "\ttime/Pullslaststep = " << (float)trueMeanTime/(UCB1.globalNumberOfPulls - prevGlobalNumberPulls)
                    << "\ttime/ Pullstotal = " << (float)totalTime/(UCB1.globalNumberOfPulls)
                << "\tGlobal Sig= " << UCB1.globalSigma
//                << "\tBest Arm local Sigma= " << localVar
//                << "\tBest Arm local estimate= " << bestArm.estimateOfMean
//                << "\tBest Arm local var= " << bestArm.estimateOfSecondMoment
//                << "\tLeft= " << groupPointsNames[left]
//                    << "\tRight= " << groupPointsNames[right]
                  << std::endl;
        prevGlobalNumberPulls = UCB1.globalNumberOfPulls;
    }

    std::chrono::system_clock::time_point timeEndEnd = std::chrono::system_clock::now();
    long long int totalTime = std::chrono::duration_cast<std::chrono::milliseconds>
            (timeEndEnd-timeStartStart).count();
    std::cout << "Total Time = " << totalTime << "(ms)"
            << "Global number of Pulls = " << UCB1.globalNumberOfPulls/(0.5*n*n)
            << "Global Sigma= " << UCB1.globalSigma
            << std::endl;

#ifdef UCB
    std::cout<< "Average distances evaluated " << UCB1.globalNumberOfPulls/(0.5*n*n) << std::endl;
#else
    std::cout<< "Average distances evaluated " << d << std::endl;
#endif
}
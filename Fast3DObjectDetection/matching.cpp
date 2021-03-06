#include "matching.h"

#include "visualize.h"
#include "utils.h"
#include "loading.h"
#include "TimeMeasuring.h"
#include "chamferScore.h"

#include <fstream>

F1Score matchInImage(int testIndex, cv::Mat &testImg_8u, FolderTemplateList &templates, HashSettings &hashSettings, std::vector<Triplet> &triplets, TemplateHashTable &hashTable, float averageEdges, int minEdges, std::vector<GroundTruth> &groundTruth, bool disableVisualisation) {
	
	cv::Mat toDraw;

	TimeMeasuring tm;
	tm.startMeasuring();

	std::vector<Candidate> candidates;

	minEdges = floor((float)minEdges * minEdgesRatio);

	
	cv::Mat sizedScene;
	testImg_8u.copyTo(sizedScene);
	blurImage(sizedScene);

	for (int i = 0; i < scalePyramidSteps; i++)
	{
		
		TimeMeasuring tm(true);
		float scaleRatio = fastPow(scalePyramidResizeRatio, i);
		
		printf("Scale pyramid - step %d @ %dx%d", i, sizedScene.cols, sizedScene.rows);
		matchInImageWithSlidingWindow(sizedScene, candidates, templates, hashSettings, triplets, hashTable, averageEdges, minEdges, scaleRatio);
		if (i < scalePyramidSteps - 1)
		{
			cv::resize(sizedScene, sizedScene, cv::Size(round(sizedScene.cols / scalePyramidResizeRatio), round(sizedScene.rows / scalePyramidResizeRatio)));
		}
		long long int ms = tm.getTimeFromBeginning();
		printf(" => in %d [ms]\n", ms);
		/*testImg_8u.copyTo(toDraw);
		for (int i = 0; i < candidates.size(); i++)
		{
			drawSlidingWindowToImage(toDraw, candidates[i].rect.width, candidates[i].rect.x, candidates[i].rect.y, candidates[i].chamferScore * 4);
		}
		cv::imshow("Possible candidates", toDraw);
		cv::waitKey();*/
	}
	std::printf("\nTotal matching time: %d [ms]\n", tm.getTimeFromBeginning());
	if (!disableVisualisation)
	{
		testImg_8u.copyTo(toDraw);
		for (int i = 0; i < candidates.size(); i++)
		{
			drawSlidingWindowToImage(toDraw, candidates[i].rect.width, candidates[i].rect.x, candidates[i].rect.y, candidates[i].chamferScore * 4);
		}
		cv::imshow("Possible candidates", toDraw);
	}
	std::printf("Total windows: %d\n", candidates.size());	

	tm.insertBreakpoint("nms");
	std::sort(candidates.begin(), candidates.end());
	nonMaximaSupression(candidates);
	long long int us = tm.getTimeFromBreakpoint("nms", true);
	std::printf("NMS time: %d [us]\n", us);

	F1Score imageScore;
	std::sort(groundTruth.begin(), groundTruth.end());
	
	cv::Mat NMS;
	if (!disableVisualisation)
	{
		testImg_8u.copyTo(NMS);
	}
	int windows = 0;
	for (int i = 0; i < candidates.size(); i++)
	{
		if (candidates[i].active)
		{
			windows++;

			int groundTruthI = solveBinarySlacification(candidates[i], groundTruth, imageScore);

			if (!disableVisualisation)
			{
				if (groundTruthI >= 0) {
					drawWindowToImage(NMS, groundTruth[groundTruthI].rect, cv::Scalar(0, 255, 0));
				}

				std::stringstream ss;
				ss << "F: " << candidates[i].tplIndex.folderIndex << " T: " << candidates[i].tplIndex.templateIndex;
				drawSlidingWindowToImage(NMS, candidates[i].rect.width, candidates[i].rect.x, candidates[i].rect.y, candidates[i].chamferScore * 4, ss.str());
				TemplateIndex &tplIndex = candidates[i].tplIndex;
				getEdgesAndDrawFullSizeToSource(NMS,
					templates[tplIndex.folderIndex][tplIndex.templateIndex].img_8u,
					candidates[i].rect.x,
					candidates[i].rect.y,
					(float)candidates[i].rect.width / (float)slidingWindowSize,1, cv::Vec3b(0,0,255));
			}
		}
	}
	
	for (int i = 0; i < groundTruth.size(); i++)
	{
		if (!groundTruth[i].active)
		{
			continue;
		}
		imageScore.falseNegative++;
		if (!disableVisualisation)
		{
			drawWindowToImage(NMS, groundTruth[i].rect, cv::Scalar(0, 255, 0));
		}
	}

	std::printf("Total windows after NMS: %d\n", windows);

	std::printf("# F1 %2.3f (Precision %1.4f / Recal: %1.4f) - TP: %2d, FP: %2d, FN: %2d\n",
		imageScore.getF1Score(true), imageScore.getPrecision(), imageScore.getRecall(),
		imageScore.truePositive, imageScore.falsePositive, imageScore.falseNegative);
	if (!disableVisualisation) {
		cv::imshow("Detection", NMS);
		cv::waitKey();
	}
	return imageScore;
}

void matchInImageWithSlidingWindow(cv::Mat &scene_8u, std::vector<Candidate> &candidates, FolderTemplateList &templates, HashSettings &hashSettings, std::vector<Triplet> &triplets, TemplateHashTable &hashTable, float averageEdges, int minEdges, float sceneScaleRatio) {
	int maxX = scene_8u.cols - slidingWindowSize + 1;
	int maxY = scene_8u.rows - slidingWindowSize + 1;

	cv::Mat edges_8u = getDetectedEdges_8u(scene_8u);

#pragma omp parallel for
	for (int x = 0; x < maxX; x += slidingWindowStep)
	{
		for (int y = 0; y < maxY; y += slidingWindowStep)
		{
			Candidate candidate = computeMatchInSlidingWindow(scene_8u, edges_8u, x, y, templates, hashSettings, triplets, hashTable, averageEdges, minEdges, sceneScaleRatio);
			if (candidate.active)
			{			
				#pragma omp critical
				candidates.push_back(candidate);
			}
		}
	}
}

DetectionUnit getDetectionUnitByROIWithQuadCount(cv::Mat &img_8u, cv::Mat &edges_8u, int x, int y, int roiSize, int minEdges, int &q1, int &q2, int &q3, int &q4) {
	DetectionUnit unit{};
	unit.edgesCount = 0;

	int quadMiddle = roiSize / 2;
	q1 = q2 = q3 = q4 = 0;
	for (int roiX = x; roiX < x + roiSize; roiX++)
	{
		for (int roiY = y; roiY < y + roiSize; roiY++)
		{
			if (edges_8u.at<uchar>(roiY, roiX) == 0) {
				if (roiX < x + quadMiddle) {
					if (roiY < y + quadMiddle) { q1++; }
					else { q2++; }
				}
				else
				{
					if (roiY < y + quadMiddle) { q3++; }
					else { q4++; }
				}
			}
		}
	}
	int edgesCount = q1 + q2 + q3 + q4;
	int lessQEdges = 0;
	if (q1 < minQuadrantEdges) { lessQEdges++; }
	if (q2 < minQuadrantEdges) { lessQEdges++; }
	if (q3 < minQuadrantEdges) { lessQEdges++; }
	if (q4 < minQuadrantEdges) { lessQEdges++; }

	if (lessQEdges > 1 || edgesCount < minEdges)
	{
		return unit;
	}

	unit.edgesCount = edgesCount;
	unit.img_8u = img_8u(cv::Rect(x, y, roiSize, roiSize));
	unit.edges_8u = edges_8u(cv::Rect(x, y, roiSize, roiSize));
	prepareDetectionUnit(unit, false, false, true);
	return unit;
}

Candidate computeMatchInSlidingWindow(cv::Mat &scene_8u, cv::Mat &edges_8u, int x, int y, FolderTemplateList &templates, HashSettings &hashSettings, std::vector<Triplet> &triplets, TemplateHashTable &hashTable, float averageEdges, int minEdges, float sceneScaleRatio) {
	int q1 = 0, q2 = 0, q3 = 0, q4 = 0;
	DetectionUnit unit = getDetectionUnitByROIWithQuadCount(scene_8u, edges_8u, x, y, slidingWindowSize, minEdges, q1, q2, q3, q4);
	if (unit.edgesCount == 0) {
		return Candidate();
	}

	tsl::hopscotch_map<TemplateIndex, int> candidatesCount;
	for (int i = 0; i < triplets.size(); i++) {
		QuantizedTripletValues hashKey = getTableHashKey(hashSettings, unit, triplets[i], i);
		if (hashTable.count(hashKey)) {
			for (int k = 0; k < hashTable[hashKey].size(); k++)
			{
				candidatesCount[hashTable[hashKey][k]]++;
			}
		}
	}
	int moreTimesThanThetaV = 0;
	float bestChamferScore = -1;
	TemplateIndex bestTemplateIndex(-1, -1);
	for (auto it = candidatesCount.begin(); it != candidatesCount.end(); it++) {
		if (it->second >= thetaV) {
			moreTimesThanThetaV++;
			float score = getOrientedChamferScore(templates[it->first.folderIndex][it->first.templateIndex], unit, averageEdges);
			if (score > bestChamferScore) {
				bestChamferScore = score;
				bestTemplateIndex = it->first;
			}
		}
	}

	if (moreTimesThanThetaV > 0 && bestChamferScore > minChamferScore) {
		//std::printf("Total candidates %4d - more than 3: %2d, best score: %4.5f\n", candidatesCount.size(), moreTimesThanThetaV, bestChamferScore);
		return Candidate(
			x * sceneScaleRatio,
			y * sceneScaleRatio,
			slidingWindowSize * sceneScaleRatio,
			bestTemplateIndex,
			bestChamferScore);
	}
	return Candidate();
}

int solveBinarySlacification(Candidate &candidate, std::vector<GroundTruth> &grounTruth, F1Score &f1score) {
	F1Score score;
	for (int i = 0; i < grounTruth.size(); i++)
	{	
		if (candidate.rect.x + candidate.rect.width <= grounTruth[i].rect.x)
		{
			break;
		}
		if (grounTruth[i].intersectOverUnion(candidate) >= GTMinOverlap)
		{
			if (candidate.folderIndex == grounTruth[i].folderIndex)
			{
				f1score.truePositive++;
			}
			else {
				f1score.falseNegative++;
			}
			grounTruth[i].active = false;
			return i;
		}
	}
	f1score.falsePositive++;
	return -1;
}

void nonMaximaSupression(std::vector<Candidate> &candidates) {
	for (int i = 0; i < candidates.size(); i++)
	{
		bool startFromBeginning = false;
		if (!candidates[i].active) { continue; }
		for (int j = i+1; j < candidates.size(); j++)
		{
			if (!candidates[j].active) { continue; }
			if (candidates[i].rect.x + candidates[i].rect.width <= candidates[j].rect.x) { break; }

			if (candidates[i].percentageOverlap(candidates[j]) >= NMSMinOverlap)
			{
				if (candidates[i].chamferScore > candidates[j].chamferScore)
				{
					candidates[j].active = false;
				}
				else
				{
					candidates[i].active = false;
					i = j;
					startFromBeginning = true;
				}
			}
		}
		if (startFromBeginning)
		{
			i = -1;
		}
	}
}
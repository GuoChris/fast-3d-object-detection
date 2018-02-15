
#include <stdio.h>
#include <tchar.h>
#include <ctime>
#include <algorithm>
#include <opencv2/opencv.hpp>

#include <iomanip>

#include <omp.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include "utils.h"

#include "DetectionUnit.h"
#include "TemplateChamferScore.h"
#include "Triplet.h"
#include "TemplateIndex.h"
#include "TripletValues.h"
#include "QuantizedTripletValues.h"
#include "HashSettings.h"
#include "TimeMeasuring.h"

#include "constantsAndTypes.h"

#include "distanceAndOrientation.h"
#include "loading.h"
#include "edgeProcessing.h"
#include "matching.h"


void prepareAndSaveData() {
	srand(time(0));
	TimeMeasuring elapsedTime(true);

	FolderTemplateList templates;
	int templatesLoaded = loadAllTemplates(templates);
	elapsedTime.insertBreakpoint("tplLoaded");
	std::printf("%d templates loaded in: %d [ms]\n", templatesLoaded, elapsedTime.getTimeFromBeginning());

	std::vector<Triplet> triplets = generateTriplets(tripletsAmount, pointsInRowCol, pointsEdgeOffset, pointsDistance);
	elapsedTime.insertBreakpoint("genTriplets");
	std::printf("Triplets generated in: %d [us] (total time: %d [ms])\n", elapsedTime.getTimeFromBreakpoint("tplLoaded", true), elapsedTime.getTimeFromBeginning());
	//visualizeTriplets(triplets, pointsEdgeOffset, pointsDistance, 48);

	TemplateHashTable hashTable;
	HashSettings hashSettings = fillHashTable(hashTable, templates, templatesLoaded, triplets, distanceBins, orientationBins);
	elapsedTime.insertBreakpoint("hashTable");
	std::printf("Hash table filled in: %d [ms] (total time: %d [ms])\n", elapsedTime.getTimeFromBreakpoint("genTriplets"), elapsedTime.getTimeFromBeginning());
	std::printf("hash table size: %d\n", hashTable.size());
	int maxSize = 0;
	for (auto it = hashTable.begin(); it != hashTable.end(); ++it) {
		if (it->second.size() > maxSize)
		{
			maxSize = it->second.size();
		}
	}
	std::printf("Max size: %d\n\n", maxSize);

	// paralel je jiz uvnitr funkce filterTemplateEdges
	// #pragma omp parallel for
	for (int f = 0; f < templates.size(); f++)
	{
		std::printf("Start %d - ", f);
		TimeMeasuring elapsedTime;
		elapsedTime.startMeasuring();
		//showResized("before", templates[f][0].edges_8u, 3);
		filterTemplateEdges(templates[f], kTpl, lambda, thetaD, thetaPhi, tau, removePixelRatio);
		//showResized("after", templates[f][0].edges_8u, 3);
		//cv::waitKey();
		std::printf("Folder %d of templates filtered in: %d [ms]\n", f, elapsedTime.getTimeFromBeginning());
	}
	elapsedTime.insertBreakpoint("filterEdges");
	std::printf("Edges filtered in: %d [ms] (total time: %d [ms])\n", elapsedTime.getTimeFromBreakpoint("hashTable"), elapsedTime.getTimeFromBeginning());

	savePreparedData("preparedData.bin", templates, triplets);

	elapsedTime.insertBreakpoint("fileSaving");
	std::printf("File saved in: %d [ms] (total time: %d [ms])\n", elapsedTime.getTimeFromBreakpoint("filterEdges"), elapsedTime.getTimeFromBeginning());

	/*FolderTemplateList templates;
	std::vector<Triplet> triplets;
	TemplateHashTable hashTable;
	HashSettings hashSettingsLoad = loadPreparedData("preparedData.bin", templates, triplets, hashTable);
	elapsedTime.insertBreakpoint("fileLoading");
	std::printf("File loaded in: %d [ms] (total time: %d [ms])\n", elapsedTime.getTimeFromBreakpoint("fileSaving"), elapsedTime.getTimeFromBeginning());
	savePreparedData("preparedData2.bin", templates, triplets);*/

	std::printf("Total time: %d [ms]\n", elapsedTime.getTimeFromBeginning());
}

void runMatching() {

	TimeMeasuring elapsedTime(true);

	std::printf("Start loading\n");

	FolderTemplateList templates;
	std::vector<Triplet> triplets;
	TemplateHashTable hashTable;
	HashSettings hashSettings;

	bool success = loadPreparedData("preparedData.bin", templates, triplets, hashTable, hashSettings);
	std::printf("Data loaded - %s\n", (success ? "sucessfully" : "with error"));

	elapsedTime.insertBreakpoint("fileLoaded");
	std::printf("File loaded in: %d [ms] (total time: %d [ms])\n", elapsedTime.getTimeFromBeginning(), elapsedTime.getTimeFromBeginning());
	
	// sliding Window
	// ud�lat v��ez, p�ipravit DetectionUnit
	// Pro nalezen� shodn�ch �ablon je pot�eba QuantizedTripletValues, ty se z�skaj� z HashSettings
	// pomoc� spo�ten�ch hodnot z triplet�
	// 
	// TemplateIndex pot�ebuje hash kv�li unordered_map (kl�� bude TemplateIndex a hodnota bude po�et v�skyt�)

	std::printf("Total time: %d [ms]\n", elapsedTime.getTimeFromBeginning());

	matchInImage(loadTestImage_8u(1), hashSettings, triplets, hashTable);
}

int main()
{
	std::cout << "Insert index of action to run:\n";
	std::cout << "\t1. Prepare data\n";
	std::cout << "\t2. Run matching\n";

	int algo;
	std::cin >> algo;
	std::cin.clear();
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	switch (algo)
	{
	case 1:
		prepareAndSaveData();
		break;
	case 2:
		runMatching();
		break;
	default:
		std::cout << "Invalid action";
	}
	
	std::getc(stdin);
	return 0;
	
	//return testDetectedEdgesAndDistanceTransform();

	//return filterTemplateEdges();
	//return selectingByStabilityToViewpoint();
	//return selectingByEdgeOrientations();

    return 0;
}
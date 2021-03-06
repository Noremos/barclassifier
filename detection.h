#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>


#include "include/barcodeCreator.h" 

#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <opencv2/ml.hpp>
/////////////////////////////
/////////////////////////////
/////////////////////////////  at ((row,col) = (hei,wid)
/////////////////////////////
/////////////////////////////

namespace fs = std::filesystem;

typedef cv::Point3_<uint8_t> Pixel;
using std::string;
using namespace cv;

int pr = 10; bool normA = false;

//Total : 1421 / 1756

const int N = 10;

std::unordered_map<string, int> categorues;

class barclassificator
{
public:
	bc::Barcontainer classes[N * 2];

	void addClass(bc::Barcontainer* cont, int classInd)
	{
		classes[classInd].addItem(cont->exractItem(0));
		classes[classInd + N].addItem(cont->exractItem(1));
		delete cont;
	}

	int check(bc::Baritem* bar0, bc::Baritem* bar255, int type)
	{
		float res = 0;

		if (classes[type].count() == 0)
		{
			return -1;
		}

		int maxInd = type;
		float maxP = res;
		for (size_t i = 0; i < N; i++)
		{
			float ps = classes[i].compireBest(bar0) * 0.5 + classes[i + N].compireBest(bar255) * 0.5;
			if (ps > maxP)
			{
				maxP = ps;
				maxInd = i;
			}
		}

		return type == maxInd ? 1 : 0;
	}

	~barclassificator()
	{	}
};

using namespace bc;
using namespace std;

bool replace(std::string& str, const std::string& from, const std::string& to) {
	size_t start_pos = str.find(from);
	if (start_pos == std::string::npos)
		return false;
	str.replace(start_pos, from.length(), to);
	return true;
}

void split(string str, std::vector<string>& strings, char splitCh = ' ')
{
	std::istringstream f(str);
	string s;
	while (getline(f, s, splitCh))
		strings.push_back(s);
}
int gt(string s)
{
	return atoi(s.c_str());
}

inline bool exists(const std::string& name) {
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

void getSet(string path, barclassificator& data, char diff = '0', float* params = nullptr) 
{
	barcodeCreator bc;
	int categoruesSize = 0;

	string labelSubpath = "/labelTxt-v1.5/DOTA-v1.5_hbb/";
	string coords = path + labelSubpath;

	string subbaseIMG = "/images/";
	string subpath = subbaseIMG;
	int ispatr = 0;
	if (exists(path + subbaseIMG + "part1"))
	{
		subpath = subbaseIMG + "part1/images/";
		ispatr = 1;
	}

	std::vector<bc::barstruct> structure;

	std::vector<bc::barstruct> constr;
	constr.push_back(barstruct(ProcType::f0t255, ColorType::gray, ComponentType::Hole));
	constr.push_back(barstruct(ProcType::f255t0, ColorType::gray, ComponentType::Hole));
	using recursive_directory_iterator = fs::recursive_directory_iterator;
	int k = 0;

	for (const auto& dirEntry : recursive_directory_iterator(coords))
	{
		string coordsPath = dirEntry.path().string();
		string	imgpath = coordsPath;
		replace(imgpath, labelSubpath, subpath);
		replace(imgpath, "txt", "png");
		if (!exists(imgpath))
		{
			ispatr += 1;
			imgpath = coordsPath;

			subpath = subbaseIMG + "part" + std::to_string(ispatr) + "/images/";
			replace(imgpath, labelSubpath, subpath);
			replace(imgpath, "txt", "png");
		}

		cv::Mat source = cv::imread(imgpath, cv::IMREAD_GRAYSCALE);

		std::ifstream infile(coordsPath);
		std::string line;

		std::getline(infile, line);
		std::getline(infile, line);
		bool stop = false;
		while (std::getline(infile, line))
		{
			std::vector<string> lids;
			split(line, lids);

			string s = (lids[8]);
			if (lids[9][0] == diff)
				continue;

			auto r = categorues.find(s);
			if (r == categorues.end())
				continue;

			int index = r->second;


			int xend = atoi(lids[2].c_str());
			int yend = atoi(lids[5].c_str());
			if (yend >= source.rows || xend >= source.cols)
				continue;

			int x = atoi(lids[0].c_str());
			int y = atoi(lids[1].c_str());
			cv::Rect ds(x, y, xend - x, yend - y);
			cv::Mat m = source(ds);
			if (m.empty())
				continue;

			cv::resize(m, m, cv::Size(32, 32));
			auto b = bc.createBarcode(m, constr);
			b->preprocessBar(pr, normA);
			addClass(b, index);
			++k;
		}

	}
	std::cout << k << endl;
}

void doMagickDOTA()
{
	string s = "../analysis/datasets/DOTA/images";
	barclassificator train;


	string whiteList = "plane, ship, storage tank, tennis court, basketball court, bridge, small vehicle, helicopter, container crane";

	std::vector<string> splited;
	split(whiteList, splited, ',');
	categorues.clear();
	int l = 0;
	for (auto& c : splited)
	{
		int soff = c[0] == ' ' ? 1 : 0;
		for (size_t i = 1; i < c.length(); i++)
		{
			if (c[i] == ' ')
			{
				c[i] = '-';
			}
		}
		categorues[c.substr(soff)] = l++;
	}

	int cN = splited.size();
	assert(cN <= N);


	string pathtrain = "../train";
	string pathvalidation = "../validation";

	getSet(pathtrain, train, '0');

	barclassificator validation2;
	getSet(pathvalidation, validation2, '0');

	bc::Barcontainer testcont;
	int correct = 0;
	int total = 0;
	int cTotal = 0, cCurrect = 0;

	float params[][3] = {{1, 0, 0}, {0,0.5,0}, {0,0,0.5}, {1, 0.5, 0.0}, {1, 0.0, 0.5}, {1, 0.5, 0.5}};
	for (size_t p = 0; p < sizeof(params) / (sizeof(int) * 3); p++)
	{
		barclassificator validation;

		getSet(pathvalidation, validation, '0', params[p]);

		cout << "Params: " << params[p][0] << " " << params[p][1] << " " << params[p][2] << endl;
		for (size_t i = 0; i < cN; i++)
		{
			auto& barc0 = validation.classes[i];
			auto& barc1 = validation.classes[N + i];
			for (size_t j = 0; j < barc0.count(); j++)
			{
				auto* bar0 = barc0.get(j);
				auto* bar1 = barc1.get(j);
				if (train.check(bar0, bar1, i))
				{
					++correct;
				}
				++total;
			}
			cTotal += total;
			cCurrect += correct;
			std::cout << " done for " << splited[i] << ": " << correct << "/" << total << std::endl;
			correct = 0;
			total = 0;
		}

		std::cout << "\nTotal: " << cCurrect << "/" << cTotal << "| " << static_cast<float>(cCurrect) / cTotal << std::endl << std::endl;
		cCurrect = 0;
		cTotal = 0;
	}
	system("pause");
}
//

//Total : 1417 / 1756 | 0.806948
//Total : 1368 / 1756 | 0.779043
//Total : 1251 / 1756 | 0.712415
//Total : 1373 / 1756 | 0.781891
//Total : 1265 / 1756 | 0.720387
//Total : 1265 / 1756 | 0.720387
//Total: 28504/34789
//Total : 1417 / 1756 | 0.806948
//Total : 2838 / 3512 | 0.808087
//Total : 4259 / 5268 | 0.808466
//Total : 5676 / 7024 | 0.808087
//Total : 7093 / 8780 | 0.807859
//Total : 8510 / 10536 | 0.807707
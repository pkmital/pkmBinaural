/*
 *  pkmBinauralizerTree.cpp
 *  autoGraffitiFLANN
 *
 *  Created by Mr. Magoo on 7/9/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "pkmBinauralizerTree.h"
#include <Accelerate/Accelerate.h>

// HRTFs
ircam_hrtf_filter_set	pkmBinauralizerTree::m_hrtfs;		// frequency domain filters

// For kNN - using C-version of FlANN
pkm::Mat				pkmBinauralizerTree::dataset;		// positions dataset
float					*pkmBinauralizerTree::query;		// position query 
int						*pkmBinauralizerTree::nnIdx;		// near neighbor indices of the dataset
float					*pkmBinauralizerTree::dists;		// near neighbor distances from the query to each dataset member
flann_index_t			pkmBinauralizerTree::kdTree;		// kd-tree reference representing all the feature-frames
struct FLANNParameters	pkmBinauralizerTree::flannParams;	// index parameters are stored here
int						pkmBinauralizerTree::k;				// number of nearest neighbors
int						pkmBinauralizerTree::dim;			// dimension of each point
int						pkmBinauralizerTree::pts;			// number of points
bool					pkmBinauralizerTree::bBuiltIndex;	// did we build the kd-tree?


// For filtering
FFTSetup				pkmBinauralizerTree::fftSetup;
size_t					pkmBinauralizerTree::filterLength, 
						pkmBinauralizerTree::paddedFilterLength, 
						pkmBinauralizerTree::convolutionLength, 
						pkmBinauralizerTree::inputLength,
						pkmBinauralizerTree::fftSizeOver2,
						pkmBinauralizerTree::log2n, 
						pkmBinauralizerTree::log2nhalf;

float					pkmBinauralizerTree::prev_x, 
						pkmBinauralizerTree::prev_y, 
						pkmBinauralizerTree::prev_z;

float					*pkmBinauralizerTree::weightedLData1,
						*pkmBinauralizerTree::weightedRData1,
						*pkmBinauralizerTree::weightedLData2,
						*pkmBinauralizerTree::weightedRData2,
						*pkmBinauralizerTree::weightedLData3,
						*pkmBinauralizerTree::weightedRData3,
						*pkmBinauralizerTree::weightedLDataSummed,
						*pkmBinauralizerTree::weightedRDataSummed,
						*pkmBinauralizerTree::previousOutputLData,
						*pkmBinauralizerTree::previousOutputRData,
						*pkmBinauralizerTree::currentOutputLData,
						*pkmBinauralizerTree::currentOutputRData;

float					*pkmBinauralizerTree::paddedFilter_l, 
						*pkmBinauralizerTree::paddedFilter_r;
COMPLEX_SPLIT			pkmBinauralizerTree::l_ir_fft,
						pkmBinauralizerTree::r_ir_fft;

bool					pkmBinauralizerTree::bInitialized;


pkmBinauralizerTree::pkmBinauralizerTree()
{
	bInitialized = bBuiltIndex = false;
	prev_x = prev_y = prev_z = 0xFFFFFFFF;
}

void pkmBinauralizerTree::deallocate()
{
	if (bInitialized) {
		// clean up KDTree
		free(nnIdx);
		free(dists);
		free(query);
		if(bBuiltIndex)
		{
			flann_free_index(kdTree, &flannParams);
			bBuiltIndex = false;
		}
				
		// clean up convolution
		vDSP_destroy_fftsetup(fftSetup);
		free(previousOutputLData);
		free(previousOutputRData);
		free(currentOutputLData);
		free(currentOutputRData);
		free(weightedLData1);
		free(weightedRData1);
		free(weightedLData2);
		free(weightedRData2);
		free(weightedLData3);
		free(weightedRData3);
		free(weightedLDataSummed);
		free(weightedRDataSummed);
		free(paddedFilter_l);
		free(paddedFilter_r);
		free(l_ir_fft.realp);
		free(l_ir_fft.imagp);
		free(r_ir_fft.realp);
		free(r_ir_fft.imagp);
		
		bInitialized = false;
	}
	
}

void pkmBinauralizerTree::initialize()
{
	
	// -----  Convolution Parameters ------
	filterLength = 512;
	inputLength = 512;
	
	// pad by one so convolution overlap is the same size as the signal (n + m - 1; overlap is (n <- m = n))
	paddedFilterLength = inputLength + 1;
	convolutionLength = inputLength + paddedFilterLength - 1;
	fftSizeOver2 = convolutionLength/2.0;
	log2n = log2f(convolutionLength);
	log2nhalf = log2n/2.0;
	fftSetup = vDSP_create_fftsetup(log2n, FFT_RADIX2);
	
	// allocate data (16 byte allignment)
	previousOutputLData = (float *)malloc(sizeof(float) * convolutionLength);
	previousOutputRData = (float *)malloc(sizeof(float) * convolutionLength);
	currentOutputLData = (float *)malloc(sizeof(float) * convolutionLength);
	currentOutputRData = (float *)malloc(sizeof(float) * convolutionLength);
	
	memset(previousOutputLData, 0, sizeof(float) * convolutionLength);
	memset(previousOutputRData, 0, sizeof(float) * convolutionLength);
	memset(currentOutputLData, 0, sizeof(float) * convolutionLength);
	memset(currentOutputRData, 0, sizeof(float) * convolutionLength);
	
	// more data (256 * 8 * 10 / 1024 = 20 KB)
	// these allow us to interpolate between filters
	weightedLData1 = (float *)malloc(sizeof(float) * paddedFilterLength);
	weightedRData1 = (float *)malloc(sizeof(float) * paddedFilterLength);
	weightedLData2 = (float *)malloc(sizeof(float) * paddedFilterLength);
	weightedRData2 = (float *)malloc(sizeof(float) * paddedFilterLength);
	weightedLData3 = (float *)malloc(sizeof(float) * paddedFilterLength);
	weightedRData3 = (float *)malloc(sizeof(float) * paddedFilterLength);
	weightedLDataSummed = (float *)malloc(sizeof(float) * paddedFilterLength);
	weightedRDataSummed = (float *)malloc(sizeof(float) * paddedFilterLength);
	
	memset(weightedLData1, 0, sizeof(float) * paddedFilterLength);
	memset(weightedRData1, 0, sizeof(float) * paddedFilterLength);
	memset(weightedLData2, 0, sizeof(float) * paddedFilterLength);
	memset(weightedRData2, 0, sizeof(float) * paddedFilterLength);
	memset(weightedLData3, 0, sizeof(float) * paddedFilterLength);
	memset(weightedRData3, 0, sizeof(float) * paddedFilterLength);
	memset(weightedLDataSummed, 0, sizeof(float) * paddedFilterLength);
	memset(weightedRDataSummed, 0, sizeof(float) * paddedFilterLength);
	
	// the final filter
	paddedFilter_l = (float *)malloc(sizeof(float)*convolutionLength);
	paddedFilter_r = (float *)malloc(sizeof(float)*convolutionLength);
	l_ir_fft.realp = (float *)malloc(sizeof(float)*convolutionLength);
	l_ir_fft.imagp = (float *)malloc(sizeof(float)*convolutionLength);
	r_ir_fft.realp = (float *)malloc(sizeof(float)*convolutionLength);
	r_ir_fft.imagp = (float *)malloc(sizeof(float)*convolutionLength);	
	
	// ------ KD-tree ---------
	
	k				= 3;						// number of nearest neighbors
	dim				= 3;						// dimension of data (x,y,z)
	pts				= 187;						// maximum number of data points
	
	nnIdx			= (int *)malloc(sizeof(int)*k);			// allocate near neighbor indices
	dists			= (float *)malloc(sizeof(float)*k);		// allocate near neighbor dists	
	query			= (float *)malloc(sizeof(float)*dim);	// pre-allocate a query frame
	dataset.reset(pts, dim, true);							// allocate our dataset
	
	for (int i = 0; i < pts; i++) {
		for (int j = 0; j < dim; j++) {
			dataset.data[i*dim + j] = hrtf_positions[i][j];
		}
	}
	
	flannParams = DEFAULT_FLANN_PARAMETERS;
	flannParams.algorithm = FLANN_INDEX_AUTOTUNED; // or FLANN_INDEX_KDTREE, FLANN_INDEX_KMEANS
	flannParams.target_precision = 0.9;
	
	if(bBuiltIndex)
	{
		flann_free_index(kdTree, &flannParams);
		bBuiltIndex = false;
	}
	
	float speedup = 2.0;
	kdTree = flann_build_index(dataset.data, pts, dim, &speedup, &flannParams);
	bBuiltIndex = true;	
}

void pkmBinauralizerTree::updateNearestFilters(float x, float y, float z)
{
	if (prev_x == x & prev_y == y && prev_z == z) {
		return;
	}
	prev_x = x;
	prev_y = y;
	prev_z = z;
	
	// convert to meters (this doesn't belong here but just for testing)
	x /= 100.0;
	y /= 100.0;
	z /= 100.0;
	
	//printf("search for neighbors at: %2.2f, %2.2f, %2.2f\n", x, y, z);
	
	// check for singularity
	// de-prioritize first by elevation
	if( x == 0.0 && y == 0.0 || x == 0.0 && z == 0.0)
	{
		x = 1.0; z = 1.0;
	}
	// so that panning is not affected
	
	// search for the nn of the query
	query[0] = x;
	query[1] = y;
	query[2] = z;	
	if (flann_find_nearest_neighbors_index_float(kdTree, 
												 query, 
												 1, 
												 nnIdx, 
												 dists, 
												 k, 
												 &flannParams) < 0)
	{
		printf("[ERROR] No frames found for Nearest Neighbor Search!\n");
		return;
	}
	
	if (k == 1) {
		memcpy(weightedLDataSummed, irc_1037.filters[nnIdx[0]].left, sizeof(float)*filterLength);
		memcpy(weightedRDataSummed, irc_1037.filters[nnIdx[0]].right, sizeof(float)*filterLength);
		
		float attenutation = powf(1.95/(dists[0] + 0.0001),3);
		vDSP_vsdiv(weightedLDataSummed, 1, &attenutation, weightedLDataSummed, 1, filterLength);
		vDSP_vsdiv(weightedRDataSummed, 1, &attenutation, weightedRDataSummed, 1, filterLength);
		
		
	}
	else if(k == 2) {
		float sumDists = dists[0] + dists[1];
		float dist1 = dists[0] / sumDists;
		float dist2 = dists[1] / sumDists;
		
		vDSP_vsmul(irc_1037.filters[nnIdx[0]].left,  1, &dist1, weightedLData1,	1, filterLength);
		vDSP_vsmul(irc_1037.filters[nnIdx[0]].right, 1, &dist1, weightedRData1,	1, filterLength);
		vDSP_vsmul(irc_1037.filters[nnIdx[1]].left,  1, &dist2, weightedLData2,	1, filterLength);
		vDSP_vsmul(irc_1037.filters[nnIdx[1]].right, 1, &dist2, weightedRData2,	1, filterLength);
		
		// add all the ffts for each the left and right channels
		vDSP_vadd(weightedLData1, 1, weightedLData2, 1, weightedLDataSummed, 1, filterLength);
		vDSP_vadd(weightedRData1, 1, weightedRData2, 1, weightedRDataSummed, 1, filterLength);
		
		// attenuate volume based on distance
		float distance = ((dists[0] + dists[1]) / 3.0) - 1.0f;
		float attenutation = powf(1.95/distance,3);
		vDSP_vsdiv(weightedLDataSummed, 1, &attenutation, weightedLDataSummed, 1, filterLength);
		vDSP_vsdiv(weightedRDataSummed, 1, &attenutation, weightedRDataSummed, 1, filterLength);
		
	}
	else if(k == 3) {	
		// turn distances into weights
		float sumDists = dists[0] + dists[1] + dists[2];
		float dist1 = dists[0] / sumDists;
		float dist2 = dists[1] / sumDists;
		float dist3 = dists[2] / sumDists;
		
		float distance = ((dists[0] + dists[1] + dists[2]) / 3.0) - 1.0f;
		
		vDSP_vsmul(irc_1037.filters[nnIdx[0]].left,  1, &dist1,	weightedLDataSummed, 1, filterLength);
		vDSP_vsmul(irc_1037.filters[nnIdx[0]].right, 1, &dist1,	weightedRDataSummed, 1, filterLength);
		vDSP_vsmul(irc_1037.filters[nnIdx[1]].left,  1, &dist2,	weightedLData2,	1, filterLength);
		vDSP_vsmul(irc_1037.filters[nnIdx[1]].right, 1, &dist2,	weightedRData2,	1, filterLength);
		vDSP_vsmul(irc_1037.filters[nnIdx[2]].left,  1, &dist3,	weightedLData3,	1, filterLength);
		vDSP_vsmul(irc_1037.filters[nnIdx[2]].right, 1, &dist3,	weightedRData3,	1, filterLength);
		
		vDSP_vadd(weightedLDataSummed, 1, weightedLData2, 1, weightedLData1, 1, filterLength);
		vDSP_vadd(weightedLData3, 1, weightedLData1, 1, weightedLDataSummed, 1, filterLength);
		
		vDSP_vadd(weightedRDataSummed, 1, weightedRData2, 1, weightedRData1, 1, filterLength);
		vDSP_vadd(weightedRData3, 1, weightedRData1, 1, weightedRDataSummed, 1, filterLength);
		
		
		// attenuate volume based on distance
		float attenutation = powf(1.95/(distance+0.0001),3);
		vDSP_vsdiv(weightedLDataSummed, 1, &attenutation, weightedLDataSummed, 1, filterLength);
		vDSP_vsdiv(weightedRDataSummed, 1, &attenutation, weightedRDataSummed, 1, filterLength);
		
	}
	
	vDSP_vclr(paddedFilter_l, 1, convolutionLength);
	vDSP_vclr(paddedFilter_r, 1, convolutionLength);
	memcpy(paddedFilter_l, weightedLDataSummed, sizeof(float)*filterLength);
	memcpy(paddedFilter_r, weightedRDataSummed, sizeof(float)*filterLength);
	
	// convert to split complex
	vDSP_ctoz((COMPLEX *)paddedFilter_l, 2, &l_ir_fft, 1, fftSizeOver2); 
	
	// fft
	vDSP_fft_zrip(fftSetup, &l_ir_fft, 1, log2n, FFT_FORWARD); 
	
	// convert to split complex
	vDSP_ctoz((COMPLEX *)paddedFilter_r, 2, &r_ir_fft, 1, fftSizeOver2); 
	
	// fft
	vDSP_fft_zrip(fftSetup, &r_ir_fft, 1, log2n, FFT_FORWARD); 
}
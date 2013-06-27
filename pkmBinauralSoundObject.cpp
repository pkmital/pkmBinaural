/*
 *  pkmBinauralSoundObject.cp
 *  Copyright Parag K Mital 2011
 *  http://pkmital.com
 
 
 
 LICENCE
 
 pkmSonicGraffiti "The Software" © Parag K Mital, parag@pkmital.com
 
 The Software is and remains the property of Parag K Mital
 ("pkmital") The Licensee will ensure that the Copyright Notice set
 out above appears prominently wherever the Software is used.
 
 The Software is distributed under this Licence: 
 
 - on a non-exclusive basis, 
 
 - solely for non-commercial use in the hope that it will be useful, 
 
 - "AS-IS" and in order for the benefit of its educational and research
 purposes, pkmital makes clear that no condition is made or to be
 implied, nor is any representation or warranty given or to be
 implied, as to (i) the quality, accuracy or reliability of the
 Software; (ii) the suitability of the Software for any particular
 use or for use under any specific conditions; and (iii) whether use
 of the Software will infringe third-party rights.
 
 pkmital disclaims: 
 
 - all responsibility for the use which is made of the Software; and
 
 - any liability for the outcomes arising from using the Software.
 
 The Licensee may make public, results or data obtained from, dependent
 on or arising out of the use of the Software provided that any such
 publication includes a prominent statement identifying the Software as
 the source of the results or the data, including the Copyright Notice
 and stating that the Software has been made available for use by the
 Licensee under licence from pkmital and the Licensee provides a copy of
 any such publication to pkmital.
 
 The Licensee agrees to indemnify pkmital and hold them
 harmless from and against any and all claims, damages and liabilities
 asserted by third parties (including claims for negligence) which
 arise directly or indirectly from the use of the Software or any
 derivative of it or the sale of any products based on the
 Software. The Licensee undertakes to make no liability claim against
 any employee, student, agent or appointee of pkmital, in connection 
 with this Licence or the Software.
 
 
 No part of the Software may be reproduced, modified, transmitted or
 transferred in any form or by any means, electronic or mechanical,
 without the express permission of pkmital. pkmital's permission is not
 required if the said reproduction, modification, transmission or
 transference is done without financial return, the conditions of this
 Licence are imposed upon the receiver of the product, and all original
 and amended source code is included in any transmitted product. You
 may be held legally responsible for any copyright infringement that is
 caused or encouraged by your failure to abide by these terms and
 conditions.
 
 You are not permitted under this Licence to use this Software
 commercially. Use for which any financial return is received shall be
 defined as commercial use, and includes (1) integration of all or part
 of the source code or the Software into a product for sale or license
 by or on behalf of Licensee to third parties or (2) use of the
 Software or any derivative of it for research with the final aim of
 developing software products for sale or license to a third party or
 (3) use of the Software or any derivative of it for research with the
 final aim of developing non-software products for sale or license to a
 third party, or (4) use of the Software to provide any service to an
 external organisation for which payment is received. If you are
 interested in using the Software commercially, please contact pkmital to
 negotiate a licence. Contact details are: parag@pkmital.com
 
 *
 */

#include "pkmBinauralSoundObject.h"
#include "pkmIRCAM_HRTF_DATABASE.h"
#include <iostream>

FFTSetup				pkmBinauralSoundObject::fftSetup;

size_t					pkmBinauralSoundObject::fftSize, 
						pkmBinauralSoundObject::fftSizeOver2, 
						pkmBinauralSoundObject::log2n, 
						pkmBinauralSoundObject::log2nhalf, 
						pkmBinauralSoundObject::bufferSize, 
						pkmBinauralSoundObject::paddedBufferSize;

COMPLEX_SPLIT			pkmBinauralSoundObject::signal_fft, 
						pkmBinauralSoundObject::l_filtered_signal_fft, 
						pkmBinauralSoundObject::r_filtered_signal_fft;

float					*pkmBinauralSoundObject::fftWindow, 
						*pkmBinauralSoundObject::windowedSignal, 
						*pkmBinauralSoundObject::left_result, 
						*pkmBinauralSoundObject::right_result, 
						*pkmBinauralSoundObject::paddedSignal;	

bool					pkmBinauralSoundObject::bInitialized;

pkmBinauralSoundObject::pkmBinauralSoundObject()
{
	bThisObjInitialized = false;
	prev_x = prev_y = prev_z = 0.0f;
};

pkmBinauralSoundObject::~pkmBinauralSoundObject()
{
	if (bThisObjInitialized) {
		
		free(prevLOverlap);
		free(prevROverlap);
	}
};

void pkmBinauralSoundObject::deallocate()
{
	if (bInitialized) {
		
		// clean up fft
		free(left_result);
		free(right_result);
		free(paddedSignal);
		free(signal_fft.realp);
		free(signal_fft.imagp);
		free(l_filtered_signal_fft.realp);
		free(l_filtered_signal_fft.imagp);
		free(r_filtered_signal_fft.realp);
		free(r_filtered_signal_fft.imagp);
		vDSP_destroy_fftsetup(fftSetup);
#ifdef DO_WINDOW
		free(fftWindow);
#endif
	}

}
void pkmBinauralSoundObject::fftInitialize()
{	
	// ----- FFT parameters ------
	bufferSize						= 512;
	fftSize							= 1024;
	paddedBufferSize				= fftSize;
	fftSizeOver2					= fftSize/2.0;
	log2n							= log2f(fftSize);
	log2nhalf						= log2n/2.0;
	fftSetup						= vDSP_create_fftsetup(log2n, FFT_RADIX2);
	
	paddedSignal					= (float *)malloc(sizeof(float) * paddedBufferSize);
	signal_fft.realp				= (float *)malloc(sizeof(float) * fftSize);
	signal_fft.imagp				= (float *)malloc(sizeof(float) * fftSize);
	l_filtered_signal_fft.realp		= (float *)malloc(sizeof(float) * fftSize);
	l_filtered_signal_fft.imagp		= (float *)malloc(sizeof(float) * fftSize);
	r_filtered_signal_fft.realp		= (float *)malloc(sizeof(float) * fftSize);
	r_filtered_signal_fft.imagp		= (float *)malloc(sizeof(float) * fftSize);
	right_result					= (float *)malloc(sizeof(float) * fftSize);
	left_result						= (float *)malloc(sizeof(float) * fftSize);
	
	
#ifdef DO_WINDOW
	/*
	 vDSP_HANN_DENORM creates a denormalized window.
	 vDSP_HANN_NORM creates a normalized window.
	 vDSP_HALF_WINDOW creates only the first (N+1)/2 points
	 */
	fftWindow						= (float *)malloc(sizeof(float) * fftSize);
	vDSP_hann_window(fftWindow, bufferSize, vDSP_HANN_NORM);
#endif
	bInitialized = true;

}

void pkmBinauralSoundObject::initialize()
{
	if (!bInitialized) {
		printf("[ERROR]: Call pkmBinauralSoundObject::fftInitialize() first!\n");
		std::exit(1);
		return;
	}
	bNoFilter = true;
	// initialize to 0 for overlap add
	prevLOverlap					= (float *)malloc(sizeof(float) * bufferSize);
	prevROverlap					= (float *)malloc(sizeof(float) * bufferSize);
	vDSP_vclr(prevLOverlap, 1, bufferSize);
	vDSP_vclr(prevROverlap, 1, bufferSize);	
	
	bThisObjInitialized = true;
}

// midi-rate
// assumes you are inside the sphere
void pkmBinauralSoundObject::updateNearestFilters(float x, float y, float z)
{
	if (x == 0 && y == 0 && z == 0) {
		bNoFilter = true;
	}
	else {
		bNoFilter = false;
		pkmBinauralizerTree::updateNearestFilters(x,y,z);
	}
}


// audio-rate
void pkmBinauralSoundObject::binauralize(float *signal, float *lOutput, float *rOutput)
{
	if (bNoFilter) {
		cblas_scopy(bufferSize, signal, 1, lOutput, 1);
		cblas_scopy(bufferSize, signal, 1, rOutput, 1);
		return;
	}
	
	float scale = 1.0 / (8.0 * (float)fftSize); 

	// copy in signal
	cblas_scopy(bufferSize, signal, 1, paddedSignal, 1);
	
	// set padding to 0
	vDSP_vclr(paddedSignal+bufferSize, 1, paddedBufferSize-bufferSize);
	
#ifdef DO_WINDOW
	// if you need to window
	vDSP_vmul(paddedSignal, 1, fftWindow, 1, paddedSignal, 1, bufferSize);
#endif
	
	// convert to split complex
	vDSP_ctoz((COMPLEX *)paddedSignal, 2, &signal_fft, 1, fftSizeOver2); 
	
	// fft
	vDSP_fft_zrip(fftSetup, &signal_fft, 1, log2n, FFT_FORWARD); 
	
	
	// ********************** Left Channel **********************
	float preserveIRNyq = pkmBinauralizerTree::l_ir_fft.imagp[0]; 
	float preserveSigNyq = signal_fft.imagp[0];
	pkmBinauralizerTree::l_ir_fft.imagp[0] = 0;  
	signal_fft.imagp[0] = 0; 
	
	// convolution of complex split data
	vDSP_zvmul(&signal_fft, 1, &pkmBinauralizerTree::l_ir_fft, 1, &l_filtered_signal_fft, 1, fftSize, 1); 
	
	l_filtered_signal_fft.imagp[0] = preserveIRNyq * preserveSigNyq; 
	pkmBinauralizerTree::l_ir_fft.imagp[0] = preserveIRNyq; 
	
	// ifft
	vDSP_fft_zrip(fftSetup, &l_filtered_signal_fft, 1, log2n, FFT_INVERSE); 
	
	vDSP_ztoc(&l_filtered_signal_fft, 1, (COMPLEX *)left_result, 2, fftSizeOver2); 
	
	// overlap and add
	vDSP_vadd(prevLOverlap, 1, left_result, 1, left_result, 1, bufferSize-1);
	cblas_scopy(bufferSize-1, left_result + bufferSize, 1, prevLOverlap, 1);
	
	// scale output (vdsp weirdness)
	vDSP_vsmul(left_result, 1, &scale, lOutput, 1, bufferSize); 
	
#ifdef DO_WINDOW
	// if you need to window:
	vDSP_vmul(lOutput, 1, fftWindow, 1, lOutput, 1, bufferSize);
#endif
	
	// ********************** Right Channel **********************
	preserveIRNyq = pkmBinauralizerTree::r_ir_fft.imagp[0]; 
	pkmBinauralizerTree::r_ir_fft.imagp[0] = 0; 
	signal_fft.imagp[0] = 0; 
	
	// convolution of complex split data (both magnitudes and phase)
	vDSP_zvmul(&signal_fft, 1, &pkmBinauralizerTree::r_ir_fft, 1, &r_filtered_signal_fft, 1, fftSize, 1); 
	
	r_filtered_signal_fft.imagp[0] = preserveIRNyq * preserveSigNyq; 
	pkmBinauralizerTree::r_ir_fft.imagp[0] = preserveIRNyq; 
	
	// ifft
	vDSP_fft_zrip(fftSetup, &r_filtered_signal_fft, 1, log2n, FFT_INVERSE); 
	
	// from split-complex to normal
	vDSP_ztoc(&r_filtered_signal_fft, 1, (COMPLEX *)right_result, 2, fftSizeOver2); 
	
	// overlap and add
	vDSP_vadd(prevROverlap, 1, right_result, 1, right_result, 1, bufferSize-1);
	cblas_scopy(bufferSize-1, right_result + bufferSize, 1, prevROverlap, 1);
	
	// scale output (vdsp weirdness)
	vDSP_vsmul(right_result, 1, &scale, rOutput, 1, bufferSize); 
	
#ifdef DO_WINDOW
	// if you need to window
	vDSP_vmul(rOutput, 1, fftWindow, 1, rOutput, 1, bufferSize);
#endif
}


void pkmBinauralSoundObject::linearConvolution(float *x, size_t x_len, float *h, size_t h_len, float *y, size_t &z_len)
{
	float sum;
	int i, j;
	z_len = (x_len + h_len - 1);
	for (i = 0; i <= z_len; i++) {
        sum = 0;
        for (j = 0; j <= h_len; j++) {
            if (j > i)
				continue;
			sum += ((h[j]) * (x[i - j]));
        }
        y[i] = sum;
	}
}
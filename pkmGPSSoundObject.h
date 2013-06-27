/* 
 
 // Parag K. Mital
 // pkmital.com
 // Copyright 2010 Parag K. Mital, All rights reserved.
 // 31.10.10
 // pkmSonicGraffiti
 
 
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
 *  general procedure:
 *

 // in header
 pkmGPSSoundObject *mySound;
 
 // in construction
 mySound = new pkmGPSSoundObject();
 mySound->encodeAudio(mySoundData, mySoundSampleLength);
 
 // in listener update
 mySound->setMyPositionRelativeToListener(xs, ys, zs, xl, yl, zl);
 
 // in audio update
 mySound->updateAudioWindow();
 mySound->updateCurrentBinauralWindow();
 float *leftChannel = getBinauralL();
 float *rightChannel = getBinauralR();
 
 */

#pragma once

#include "pkmBinauralSoundObject.h"
#include "ofMain.h"
#include "pkmRecorder.h"
#include "gps.h"
// change this when in release mode
#ifndef DEBUG
#define DEBUG 0
#endif

#define DO_3D 0
#define MINIMUM_DISTANCE 0.1f		// meters
#define INAUDIBLE_DISTANCE 100.0f	// meters
#define AMBISONIC_ORDER 1

typedef ofPoint CartesianPoint;

class pkmGPSSoundObject
{
public:
	
	// not sure that anything other than 44100 and 512 will work with amblib and the iphone
	pkmGPSSoundObject(int sr = 44100, int ws = 512)
	{
		bAudible = false;
		
		sample_rate = sr;
		window_size = ws;
		
		// allocate for processing
		window = (float *)malloc(sizeof(float) *window_size);
		binauralL = (float *)malloc(sizeof(float) *window_size);
		binauralR = (float *)malloc(sizeof(float) *window_size);
		
		// current sample to start windowing of the buffer
		current_step = 0;
		
		// setup the binauralizer
		binauralizer.initialize();
		
		// did we store a sound file yet?
		buffer_length = 0;
		bEncoded = false;
	}
	
	virtual ~pkmGPSSoundObject()
	{
		if (bEncoded) {
			free(data);
		}
		free(window);
		free(binauralR);
		free(binauralL);
		
	}
	
	virtual bool encoded()
	{
		return bEncoded;
	}
	
	virtual void encodeAudio(pkmRecorder *recordedSound)
	{
		encodeAudio(recordedSound->data, recordedSound->size);
	}
	
	// store the sample's sound data locally (w/ copy, non-destructive)
	virtual void encodeAudio(float *sound_data, 
					 int sound_buffer_length)
	{
		if (bEncoded && buffer_length != sound_buffer_length) {
			printf("Re-allocating sound buffer!");
			free(data);
			buffer_length = 0;
			bEncoded = false;
		}
		if (!bEncoded) {
			data = (float *)malloc(sizeof(float)*sound_buffer_length);
			buffer_length = sound_buffer_length;
			bEncoded = true;
		}
		
		memcpy(data, sound_data, sound_buffer_length*sizeof(float));
		
	}
	
	virtual void setMyAbsoluteGPS(double lat, double lon)
	{
		latitude = lat;
		longitude = lon;
	}
	
	virtual void setMyAbsolutePosition(float absolute_sound_x, 
							   float absolute_sound_y, 
							   float absolute_sound_z)
	{
		absolute_cartposition.x = absolute_sound_x;
		absolute_cartposition.y = absolute_sound_y;
		absolute_cartposition.z = absolute_sound_z;
		
		updateMyPosition();
	}
	
	virtual void setListenersAbsolutePosition(float absolute_listener_x, 
									  float absolute_listener_y, 
									  float absolute_listener_z,
									  float orientation_listener_z = 0.0f)
	{
		orientation_listener_z = orientation_listener_z * PI / 180.0f;
		float x = absolute_cartposition.x - absolute_listener_x;
		float y = absolute_cartposition.y - absolute_listener_y;
		float z = absolute_cartposition.z - absolute_listener_z;
		relative_cartposition.x = (x * cosf(orientation_listener_z) - y * sinf(orientation_listener_z));
		relative_cartposition.y = (x * sinf(orientation_listener_z) + y * cosf(orientation_listener_z));
		relative_cartposition.z = z;
				
		updateMyPosition();
	}
	
	// set sound sources position 
	// (x,y,z is relative to listener)
	virtual void updateMyPosition()
	{
		binauralizer.updateNearestFilters(relative_cartposition.x, relative_cartposition.y, relative_cartposition.z);
	}
	
	virtual void updateCurrentBinauralWindow()
	{	
		// get the next window ready for processing
		updateAudioWindow();
		
		float dist = getDistanceFromListener();
		if (dist < INAUDIBLE_DISTANCE) {
			bAudible = true;
			if (dist < MINIMUM_DISTANCE) {
				cblas_scopy(window_size, window, 1, binauralL, 1);
				cblas_scopy(window_size, window, 1, binauralR, 1);
			}
			else {
				binauralizer.binauralize(window, binauralL, binauralR);
			}
		}
		else {
			bAudible = false;
		}

	}
	
	virtual float getDistanceFromListener()
	{
		return sqrtf( relative_cartposition.x * relative_cartposition.x + 
					  relative_cartposition.y * relative_cartposition.y + 
					  relative_cartposition.z * relative_cartposition.z );
	}
	
	virtual inline bool isAudible()
	{
		return bAudible;
	}
	
	virtual float* getBinauralL()
	{
		return binauralL;
	}
	
	virtual float* getBinauralR()
	{
		return binauralR;
	}
	
	virtual CartesianPoint getCurrentRelativePosition()
	{
		return relative_cartposition;
	}
	virtual CartesianPoint getCurrentAbsolutePosition()
	{
		return absolute_cartposition;
	}
	
	virtual inline double getLatitude()
	{
		return latitude;
	}
	
	virtual inline double getLongitude()
	{
		return longitude;
	}
	
	virtual int getBufferLength()
	{
		return buffer_length;
	}
	/*
	void getCurrentAmbisonicWindow(float *ch1, float *ch2,		// front high left, front high right
								   float *ch3, float *ch4,		// back high left,	back high right
								   float *ch5, float *ch6,		// front low left,  front low right
								   float *ch7, float *ch8)		// back low left,	back low right
	{
		
	}
	*/
protected:

	pkmBinauralSoundObject			binauralizer;
	
	CartesianPoint					relative_cartposition;
	CartesianPoint					absolute_cartposition;
	
	double							latitude, 
									longitude;
	
	bool							bAudible;
	bool							bEncoded;
		
	int								current_step;
	float							*window;
	float							*binauralL, *binauralR;
	unsigned int					window_size;
	
	float							*data;
	unsigned int					buffer_length;

	unsigned int					sample_rate;
	
	
private:
	
	
	
	// encode the current window 
	virtual void updateAudioWindow()
	{
		if (bEncoded) 
		{
			// get the current window as a pointer
			for (int i = 0; i < window_size; i++) {
				window[i] = data[(i+current_step) % buffer_length];
			}
			
			// increment window for next iteration
			current_step = (current_step + window_size) % buffer_length;
		}
	}
	
	

					 
};
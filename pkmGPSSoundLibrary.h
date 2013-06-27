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

#include "pkmGPSSoundObject.h"
#include "pkmLiveSoundObject.h"
#include <Accelerate/Accelerate.h>
#include <vector.h>
#include "ofxiPhoneExtras.h"
#include "UTM.h"

#define DO_INTERLEAVED 1

class pkmGPSSoundLibrary
{
public:
	pkmGPSSoundLibrary(int sr = 44100, int ws = 512)
	{
		prev_x = prev_y = prev_z = prev_ori = 0xFFFFFFFF;
		sample_rate = sr;
		window_size = ws;
		
		bEncodedLiveObject = false;
		//liveSignalObject = new pkmLiveSoundObject(sample_rate, window_size);
		
		/*
		liveSignalData = new float[ws];
		memset(liveSignalData, 0, sizeof(float)*ws);
		liveSignalObject->encodeAudio(liveSignalData, ws);
		
		soundLibrary.push_back(liveSignalObject);
		*/
		numAudibleSounds = 0;
		soundLibrary.resize(0);
		
#if(DO_INTERLEAVED)
		currentBuffer = (float *)malloc(sizeof(float) *window_size*2);
#else
		// allocate for processing
		binauralL = (float *)malloc(sizeof(float) *window_size);
		binauralR = (float *)malloc(sizeof(float) *window_size);
#endif
	}
	~pkmGPSSoundLibrary()
	{
		for(vector<pkmGPSSoundObject *>::iterator it = soundLibrary.begin(); it != soundLibrary.end(); it++)
		{
			delete *it;
		}	
#if(DO_INTERLEAVED)
		free(currentBuffer);
#else
		free(binauralR);
		free(binauralL);
#endif
		free(liveSignalData);
	}
	
	void addLiveSound(float *sound_data, 
					  int num_samples, 
					  float pos_x = 0.0f, 
					  float pos_y = 0.0f, 
					  float pos_z = 0.0f,
					  double lat = 0.0,
					  double lon = 0.0)
	{
		if (!bEncodedLiveObject) {
			liveSignalObject = new pkmLiveSoundObject(sample_rate, window_size);
			liveSignalObject->encodeAudio(sound_data, num_samples);
			liveSignalObject->setMyAbsolutePosition(pos_x, pos_y, pos_z);
			liveSignalObject->setMyAbsoluteGPS(lat, lon);
			soundLibrary.push_back(liveSignalObject);	
			bEncodedLiveObject = true;
		}
		else {
			liveSignalObject->encodeAudio(sound_data, num_samples);
			liveSignalObject->setMyAbsolutePosition(pos_x, pos_y, pos_z);
		}
		//liveSignalObject->encodeAudio(sound_data, ws);
		//liveSignalObject->setMyAbsolutePosition(pos_x, pos_y, pos_z);
		//bEncodedLiveObject = true;
	}
	
	void addSound(float *sound_data, int num_samples, 
				  float pos_x, 
				  float pos_y, 
				  float pos_z,
				  double lat = 0.0,
				  double lon = 0.0)
	{
		pkmGPSSoundObject *soundObject = new pkmGPSSoundObject(sample_rate, window_size);
		soundObject->encodeAudio(sound_data, num_samples);
		soundObject->setMyAbsolutePosition(pos_x, pos_y, pos_z);
		soundObject->setMyAbsoluteGPS(lat, lon);
		soundLibrary.push_back(soundObject);		
	}
	
	void setListenersAbsolutePosition(float x, 
							 float y, 
							 float z,
							 float orientation = 0.0f)
	{
		if (x == prev_x &&
			y == prev_y &&
			orientation == prev_ori &&
			z == prev_z) {
			return;
		}
		
		prev_x = x;
		prev_y = y;
		prev_z = z;
		prev_ori = orientation;
		
		for(vector<pkmGPSSoundObject *>::iterator it = soundLibrary.begin(); it != soundLibrary.end(); it++)
		{
			(*it)->setListenersAbsolutePosition(x, y, z, orientation);
		}
		//if (bEncodedLiveObject) {
		//	liveSignalObject->setListenersAbsolutePosition(x, y, z, orientation);
		//}
	}

	void drawSoundRadar(int x, int y)
	{
		
		if(getNumSounds() >= 1)
		{
			char buf[80];
			float wover2 = ofGetScreenWidth()/2.0f;
			float hover2 = ofGetScreenHeight()/2.0f;
			float radius = hover2/1.25f;
			
			float draw_radius = (float)INAUDIBLE_DISTANCE * radius;
			
			ofPushStyle();
			ofPushMatrix();
			ofTranslate(wover2, hover2);
			ofEnableAlphaBlending();
			
			// circle outline of radar
			//ofNoFill();
			//ofSetColor(255, 255, 255);
			//ofRotate(10, 1, 0, 0);
			//ofCircle(0, 0, radius);
			// circle fill
			//ofFill();
			//ofSetColor(30, 100, 200, 100);
			//ofCircle(0, 0, radius);
			
			int i = 1;
			float x,y,z;
			for(vector<pkmGPSSoundObject *>::iterator it = soundLibrary.begin(); it != soundLibrary.end(); it++)
			{
				ofPushMatrix();

				x = -(*it)->getCurrentRelativePosition().x*10.0f;
				y = -(*it)->getCurrentRelativePosition().y*10.0f;
				z = (*it)->getCurrentRelativePosition().z*10.0f;

				
				//printf("%f, %f, %f\n", x, y, z);
				ofRotate(-90, 0, 0, 1);
				ofTranslate(x, y, z);
				// individual sound circle outline
				ofNoFill();
				ofSetColor(255, 255, 255, 200);
				ofCircle(0, 0, 10);
				// and fill
				ofFill();
				ofSetColor(30, 100, 200, 200);
				ofCircle(0, 0, 10);
				// text of coordinate
				//ofTranslate(x, y, z);
				ofRotate(90, 0, 0, 1);
				ofSetColor(255, 255, 255);
				
				sprintf(buf, "%d", i++);
				ofDrawBitmapString(buf, 0, 0);
				
				//sprintf(buf, "(%2.2f,%2.2f,%2.2f)", x,y,z);
				//ofDrawBitmapString(buf, 10, 10);
				ofPopMatrix();	
			}
				
			ofDisableAlphaBlending();
			ofPopMatrix();
			ofPopStyle();
			
		}
	}
	
	void drawGPSSoundRadar(int x, int y, ofxiPhoneMapKit *mapKit)
	{
		
		if(getNumSounds() >= 1)
		{
			char buf[80];
			float wover2 = ofGetScreenWidth()/2.0f;
			float hover2 = ofGetScreenHeight()/2.0f;
			float radius = hover2/1.25f;
			
			ofPushStyle();
			ofPushMatrix();
			//ofTranslate(wover2, hover2);
			ofEnableAlphaBlending();
			
			// circle outline of radar
			//ofNoFill();
			//ofSetColor(255, 255, 255);
			//ofRotate(10, 1, 0, 0);
			//ofCircle(0, 0, radius);
			// circle fill
			//ofFill();
			//ofSetColor(30, 100, 200, 100);
			//ofCircle(0, 0, radius);
			//ofTranslate(-wover2, -hover2);
			
			int i = 1;
			float x,y,z = 0;
			double lat;
			double lon;
			for(vector<pkmGPSSoundObject *>::iterator it = soundLibrary.begin(); it != soundLibrary.end(); it++)
			{
				ofPushMatrix();
				
				
				lat = (*it)->getLatitude();
				lon = (*it)->getLongitude();
				ofPoint loc = mapKit->getScreenCoordinatesForLocation(lat, lon);
				x = loc.x;
				y = loc.y;
				
				//printf("%f, %f, %f\n", x, y, z);
				//ofRotate(-90, 0, 0, 1);
				ofTranslate(x, y, z);
				// individual sound circle outline
				ofNoFill();
				ofSetColor(255, 255, 255, 255);
				ofCircle(0, 0, 10);
				// and fill
				ofFill();
				ofSetColor(30, 100, 200, 255);
				ofCircle(0, 0, 10);
				// text of coordinate
				//ofTranslate(x, y, z);
				//ofRotate(90, 0, 0, 1);
				ofSetColor(255, 255, 255, 255);
				
				sprintf(buf, "%d", i++);
				ofDrawBitmapString(buf, 0, 0);
				
				//sprintf(buf, "(%2.2f,%2.2f,%2.2f)", x,y,z);
				//ofDrawBitmapString(buf, 10, 10);
				ofPopMatrix();	
			}

			
			ofDisableAlphaBlending();
			ofPopMatrix();
			ofPopStyle();
			
		}
	}
	
	
	
	void audioRequested()
	{
		vector<float *> leftChannels;
		vector<float *> rightChannels;
		for(vector<pkmGPSSoundObject *>::iterator it = soundLibrary.begin(); it != soundLibrary.end(); it++)
		{
			(*it)->updateCurrentBinauralWindow();
			if ((*it)->isAudible()) 
			{
				leftChannels.push_back((*it)->getBinauralL());
				rightChannels.push_back((*it)->getBinauralR());
			}
		}
		
		numAudibleSounds = leftChannels.size();
		if (numAudibleSounds > 0) 
		{
	#if(DO_INTERLEAVED)
			memset(currentBuffer, 0, sizeof(float)*2*window_size);
			
			float *bufferPtr = currentBuffer;
			for (int i = 0; i < window_size; i++) 
			{
				for(int s = 0; s < numAudibleSounds; s++)
				{
					bufferPtr[0] += leftChannels[s][i] * 0.71f;
					bufferPtr[1] += rightChannels[s][i] * 0.71f;
				}

				bufferPtr += 2;
			}
	#else
			memset(binauralL, 0, sizeof(float)*window_size);
			memset(binauralR, 0, sizeof(float)*window_size);
			for (int i = 0; i < window_size; i++) 
			{
				for(int s = 0; s < numAudibleSounds; s++)
				{
					binauralL[i] += leftChannels[s][i] * 0.71f;
					binauralR[i] += rightChannels[s][i] * 0.71f;
				}
			}
	#endif
		}
		else 
		{
#if(DO_INTERLEAVED)
			memset(currentBuffer, 0, sizeof(float)*2*window_size);
#else
			memset(binauralL, 0, sizeof(float)*window_size);
			memset(binauralR, 0, sizeof(float)*window_size);
#endif
		}

	}

#if(DO_INTERLEAVED)
		float* getCurrentBuffer()
		{	
			return currentBuffer;
		}
#else
		float* getBinauralL()
		{
			return binauralL;
		}
		
		float* getBinauralR()
		{
			return binauralR;
		}
#endif
	
	int getNumSounds()
	{
		return soundLibrary.size();// + (bEncodedLiveObject ? 1 : 0);
	}
	
	
	int	getNumAudibleSounds()
	{
		return numAudibleSounds;
	}
	
private:
	pkmLiveSoundObject				*liveSignalObject;
	float							*liveSignalData;
	vector<pkmGPSSoundObject *>		soundLibrary;
	int								sample_rate;
	int								window_size;
	int								numAudibleSounds;
	float							prev_x,prev_y,prev_z,prev_ori;
	
	float							*binauralL, *binauralR, *currentBuffer;
	bool							bEncodedLiveObject;
	
};

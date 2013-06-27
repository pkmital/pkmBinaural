/*
 *  pkmBinauralAudioPlayer.h
 *  memoryMosaic
 *
 *  Created by Mr. Magoo on 5/24/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 
 Windows playback
 Loop functionality, or stop at end 
 
 */

#include "pkmGPSAudioFile.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "pkmAudioWindow.h"
#include "pkmBinauralSoundObject.h"
#include <Accelerate/Accelerate.h>

#define MIN_FRAMES 2

#ifndef MIN
#define MIN(X,Y) (X) > (Y) ? (Y) : (X)
#endif

class pkmBinauralAudioPlayer
{
public:
	pkmBinauralAudioPlayer(pkmGPSAudioFile *myFile, 
						   int frame_size = 512, 
						   int num_frames_to_play = 0, 
						   bool loop = true)
	{
		gpsAudioFile = myFile;			
		assert((gpsAudioFile->length - gpsAudioFile->offset) > 0);
		
		//printf("gpsAudioFile size: %d, offset: %d\n", gpsAudioFile.length, gpsAudioFile.offset);
		frameSize = frame_size;		
		if (loop) {
			framesToPlay = num_frames_to_play;
		}
		else {
			framesToPlay = MIN(num_frames_to_play, (myFile->length - myFile->offset) / frame_size);
		}
		
		currentFrame = 0;
		bLoop = loop;
		//empty = (float *)malloc(sizeof(float) * frameSize);
		rampedBuffer = (float *)malloc(sizeof(float) * frameSize);
		//memset(empty, 0, sizeof(float)*frameSize);
	}
	~pkmBinauralAudioPlayer()
	{
		//free(empty);
		free(rampedBuffer);
	}
	bool initialize()
	{
		// default play to the end of the file from the starting offset (if it exists)
		if (framesToPlay < MIN_FRAMES ||
			// if the frames to play are too high
			(gpsAudioFile->length < (gpsAudioFile->offset + frameSize*framesToPlay))) 
		{
			framesToPlay = (gpsAudioFile->length - gpsAudioFile->offset) / frameSize;
		}
		
		if (framesToPlay < MIN_FRAMES) 
		{
			framesToPlay = 0;
			currentFrame = 0;
			//printf("[ERROR]: audioPlayer failed to initialized - Not enough frames.");
			return false;
		}
		
		binauralSoundObject.initialize();
		//printf("audioPlayer initialized with %d frames to play\n", framesToPlay);
		// start at the first frame (past the offset if any)
		currentFrame = 0;
		return true;
	}
	
    inline bool isLastFrame()
    {
        return (currentFrame == framesToPlay-1);
    }
	
	// get the next audio frame to play 
	float * getNextFrame()
	{
		int offset = gpsAudioFile->offset + currentFrame*frameSize;
		if (bLoop) {
			currentFrame = (currentFrame + 1) % framesToPlay;
		}
		else {
			currentFrame++;
			if (currentFrame > framesToPlay) {
				// shouldn't get here so long as the user checks for:
				// isLastFrame() before getNextFrame()
				printf("[ERROR]: EOF reached! Returning an empty buffer!");
				vDSP_vclr(rampedBuffer, 1, frameSize);
				return rampedBuffer;
			}
		}
		// fade in
		if (currentFrame == 1) {
			//cblas_scopy(frameSize, gpsAudioFile.buffer + offset, 1, rampedBuffer, 1);
			vDSP_vmul(gpsAudioFile->buffer + offset, 1, 
					  pkmAudioWindow::rampInBuffer, 1, 
					  rampedBuffer, 1, frameSize);
			//printf("%d/%d: %f\n", currentFrame, framesToPlay, rampedBuffer[0]);
			return rampedBuffer;
		}
		// fade out
		else if(currentFrame == framesToPlay || currentFrame == 0) {
			//printf("f\n");
			//cblas_scopy(frameSize, gpsAudioFile.buffer + offset, 1, rampedBuffer, 1);
			vDSP_vmul(gpsAudioFile->buffer + offset, 1, 
					  pkmAudioWindow::rampOutBuffer, 1, 
					  rampedBuffer, 1, frameSize);
			//printf("%d/%d: %f\n", currentFrame, framesToPlay, rampedBuffer[frameSize-1]);
			return rampedBuffer;			
		}
		// no fade
		else
			return (gpsAudioFile->buffer + offset);
	}
	
	// change the sound's location
	void updateAbsoluteLocation(float px, float py, float pz)
	{
		binauralSoundObject.updateNearestFilters(px, py, pz);
	}

	// change the listener's position, using the sound's inherent position as the relative distance
	void updateRelativeLocation(float px, float py, float pz)
	{
		binauralSoundObject.updateNearestFilters(px-gpsAudioFile->position_x, 
												 py-gpsAudioFile->position_y, 
												 pz-gpsAudioFile->position_z);
	}
	
	void getBinauralFrames(float *left, float *right)
	{
		float *frame = getNextFrame();
		binauralSoundObject.binauralize(frame, left, right);
	}
	
	bool isFinished()
	{
		return !bLoop && currentFrame > framesToPlay;
	}
	
	pkmBinauralSoundObject	binauralSoundObject;
	pkmGPSAudioFile			*gpsAudioFile;
	
	int						frameSize;
	int						framesToPlay;
	int						currentFrame;
	bool					bLoop;
	int						rampInLength, rampOutLength;
	float					*empty, *rampedBuffer;
	
};

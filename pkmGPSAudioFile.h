/*
 *  pkmGPSAudioFile.h
 *  autoGrafittiMapKit
 *
 *  Created by Mr. Magoo on 5/12/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#pragma once

class pkmGPSAudioFile
{
public:
	pkmGPSAudioFile(int fs = 512)
	{
		buffer = 0;
		offset = 0;
		length = 0;
		weight = 0;
		position_x = 0;
		position_y = 0;
		position_z = 0;
		bPlaying = false;
		frame_size = fs;
	}
	
	pkmGPSAudioFile(float *&buf, int pos, int size, 
					float w = 1.0, int fs = 512, 
					float px = 0, float py = 0, float pz = 0,
					bool bP = false)
	{
		buffer = buf;
		offset = pos;
		length = size;
		weight = w;
		frame_size = fs;
		position_x = px;
		position_y = py;
		position_z = pz;
		bPlaying = bP;
	}
	
	virtual ~pkmGPSAudioFile()
	{
		buffer = 0;
		weight = offset = length = 0;
		position_x = 0;
		position_y = 0;
		position_z = 0;
		bPlaying = false;
	}
	
	pkmGPSAudioFile(const pkmGPSAudioFile &rhs)
	{
		buffer = rhs.buffer;
		offset = rhs.offset;
		length = rhs.length;
		weight = rhs.weight;
		frame_size = rhs.frame_size;
		
		position_x = rhs.position_x;
		position_y = rhs.position_y;
		position_z = rhs.position_z;
		
		bPlaying = rhs.bPlaying;
	}
	
	int getNumFrames()
	{
		return (length - offset) / frame_size;
	}
	
	
	float		*buffer;
	float		weight;
	int			offset, 
				length;
	
	int			frame_size;
	
	bool		bPlaying;
	
	float		position_x, 
				position_y, 
				position_z;
};
#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <vector>
#include <mutex>
#include "TPCircularBuffer.h"
#include "ofMain.h"
class ofxAudioUnit;

class ofxAudioUnitDSPNode
{
public:
	ofxAudioUnitDSPNode(unsigned int samplesToBuffer = 2048);
	virtual ~ofxAudioUnitDSPNode();
	
	ofxAudioUnit& connectTo(ofxAudioUnit &destination, int destinationBus = 0, int sourceBus = 0);
	ofxAudioUnitDSPNode& connectTo(ofxAudioUnitDSPNode &destination, int destinationBus = 0, int sourceBus = 0);
	
	void setSource(ofxAudioUnit * source);
	void setSource(AURenderCallbackStruct callback, UInt32 channels = 2);
	
	AudioStreamBasicDescription getSourceASBD(int sourceBus = 0) const;
	
	
	ofxAudioUnit * getSourceAU();
	ofxAudioUnitDSPNode* getSourceDSPNode();
	
	
	virtual string getName();
	
	string name;


	typedef enum
	{
		NodeSourceNone,
		NodeSourceUnit,
		NodeSourceCallback
	}
	ofxAudioUnitDSPNodeSourceType;
	
	
	struct DSPNodeContext
	{
		ofxAudioUnitDSPNodeSourceType sourceType;
		ofxAudioUnit * sourceUnit = nullptr;
		ofxAudioUnitDSPNode  * sourceDSPNode = nullptr;
		UInt32 sourceBus;
		AURenderCallbackStruct sourceCallback;
		AURenderCallbackStruct processCallback;
		std::vector<TPCircularBuffer> circularBuffers;
		std::mutex bufferMutex;
		
		DSPNodeContext();
		void setCircularBufferSize(UInt32 bufferCount, unsigned int samplesToBuffer);
		
	private:
		unsigned int _bufferSize;
	};
	struct NodeImpl
	{
		NodeImpl(unsigned int samplesToBuffer, unsigned int channelsToBuffer):
			samplesToBuffer(samplesToBuffer),
			channelsToBuffer(channelsToBuffer)
		{}
		
		DSPNodeContext ctx;
		unsigned int samplesToBuffer;
		unsigned int channelsToBuffer;
	};
	void setSourceDSPNode(ofxAudioUnitDSPNode* source);
protected:
	std::shared_ptr<NodeImpl> _impl;

	void getSamplesFromChannel(std::vector<Float32> &samples, unsigned int channel) const;
	
	
	
	// sets the internal circular buffer size
	void setBufferSize(unsigned int samplesToBuffer);
	unsigned int getBufferSize() const;
	
	// sets a callback that will be called every time audio is
	// passed through the node (note: this will be called on the
	// render thread)
	void setProcessCallback(AURenderCallbackStruct processCallback);
	
	
	
	
	
};

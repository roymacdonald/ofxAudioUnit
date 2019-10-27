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
	
	virtual ofxAudioUnit& connectTo(ofxAudioUnit &destination, int destinationBus = 0, int sourceBus = 0);
	virtual ofxAudioUnitDSPNode& connectTo(ofxAudioUnitDSPNode &destination, int destinationBus = 0, int sourceBus = 0);
	
	void setSource(ofxAudioUnit * source);
	void setSource(AURenderCallbackStruct callback, UInt32 channels = 2);
	
	
	
	typedef enum
	{
		NodeSourceNone,
		NodeSourceUnit,
		NodeSourceCallback
	}
	ofxAudioUnitDSPNodeSourceType;
	
	size_t getTicks(){
		std::lock_guard<std::mutex> lck(timeStampMutex);
		return _impl->ctx.ticks;
	}
	
	AudioTimeStamp getCurrentTimeStamp(){
		std::lock_guard<std::mutex> lck(timeStampMutex);
		return _impl->ctx.currentTimeStamp;
	}
	
	struct DSPNodeContext
	{
		
		AudioTimeStamp currentTimeStamp;
		size_t ticks;
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
	AudioStreamBasicDescription getSourceASBD(int sourceBus = 0) const;
	
	ofxAudioUnit * getSourceAU(){
		if(_impl->ctx.sourceType == ofxAudioUnitDSPNode::NodeSourceUnit ){
			return _impl->ctx.sourceUnit;
		}
		return NULL;
	}
	ofxAudioUnitDSPNode* getSourceDSPNode(){
		if(_impl->ctx.sourceType == ofxAudioUnitDSPNode::NodeSourceCallback) {
			return _impl->ctx.sourceDSPNode;
//			return static_cast<ofxAudioUnitDSPNode::DSPNodeContext *> (_impl->ctx.sourceCallback.inputProcRefCon);
		}
		return NULL;
	}
	void setSourceAU(ofxAudioUnit * source){
		_impl->ctx.sourceDSPNode = nullptr;
		_impl->ctx.sourceUnit = source;
	}
	void setSourceDSPNode(ofxAudioUnitDSPNode* source){
		_impl->ctx.sourceDSPNode = source;
		_impl->ctx.sourceUnit = nullptr;
	}
	
	
	virtual string getName(){
		if(name.empty()){
			return "DSPNode";
		}else{
			return name;
		}
	}
	
	string name;

	
	
	struct NodeImpl
	{
		NodeImpl(unsigned int samplesToBuffer, unsigned int channelsToBuffer)
		: samplesToBuffer(samplesToBuffer)
		, channelsToBuffer(channelsToBuffer)
		{
			
		}
		
		DSPNodeContext ctx;
		unsigned int samplesToBuffer;
		unsigned int channelsToBuffer;
	};
	std::shared_ptr<NodeImpl> _impl;
protected:
	void getSamplesFromChannel(std::vector<Float32> &samples, unsigned int channel) const;
	
	// sets the internal circular buffer size
	void setBufferSize(unsigned int samplesToBuffer);
	unsigned int getBufferSize() const;
	
	// sets a callback that will be called every time audio is
	// passed through the node (note: this will be called on the
	// render thread)
	void setProcessCallback(AURenderCallbackStruct processCallback);
	
	
	std::mutex timeStampMutex;
	
	
	
};

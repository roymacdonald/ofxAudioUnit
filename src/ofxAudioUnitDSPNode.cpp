#include "ofxAudioUnitDSPNode.h"
#include "ofxAudioUnitBase.h"
#include "ofxAudioUnitUtils.h"


// a passthru render callback which copies the rendered samples in the process
static OSStatus RenderAndCopy(void * inRefCon,
							  AudioUnitRenderActionFlags *	ioActionFlags,
							  const AudioTimeStamp *	inTimeStamp,
							  UInt32 inBusNumber,
							  UInt32	inNumberFrames,
							  AudioBufferList * ioData);

// a render callback which just outputs silence and doesn't require a context
static OSStatus SilentRenderCallback(void * inRefCon,
									 AudioUnitRenderActionFlags *	ioActionFlags,
									 const AudioTimeStamp *	inTimeStamp,
									 UInt32 inBusNumber,
									 UInt32	inNumberFrames,
									 AudioBufferList * ioData);


	ofxAudioUnitDSPNode::DSPNodeContext::DSPNodeContext()
	: sourceBus(0)
	, sourceType(NodeSourceNone)
	, sourceCallback((AURenderCallbackStruct){0})
	, processCallback((AURenderCallbackStruct){0})
	, sourceUnit(NULL)
	, _bufferSize(0)
	{ }
	
	void ofxAudioUnitDSPNode::DSPNodeContext::setCircularBufferSize(UInt32 bufferCount, unsigned int samplesToBuffer) {
		if(bufferCount != circularBuffers.size() || samplesToBuffer != _bufferSize) {
			bufferMutex.lock();
			{
				for(int i = 0; i < circularBuffers.size(); i++) {
					TPCircularBufferCleanup(&circularBuffers[i]);
				}
				
				circularBuffers.resize(bufferCount);
				
				for(int i = 0; i < circularBuffers.size(); i++) {
					TPCircularBufferInit(&circularBuffers[i], samplesToBuffer * sizeof(Float32));
				}
				_bufferSize = samplesToBuffer;
			}
			bufferMutex.unlock();
		}
	}
	

// ----------------------------------------------------------
ofxAudioUnitDSPNode::ofxAudioUnitDSPNode(unsigned int samplesToBuffer)
: _impl(new NodeImpl(samplesToBuffer, 2))
// ----------------------------------------------------------
{
	
}

// ----------------------------------------------------------
ofxAudioUnitDSPNode::~ofxAudioUnitDSPNode()
// ----------------------------------------------------------
{
	for(int i = 0; i < _impl->ctx.circularBuffers.size(); i++) {
		TPCircularBufferCleanup(&_impl->ctx.circularBuffers[i]);
	}
}

#pragma mark - Connections

// ----------------------------------------------------------
ofxAudioUnit& ofxAudioUnitDSPNode::connectTo(ofxAudioUnit &destination, int destinationBus, int sourceBus)
// ----------------------------------------------------------
{
	// checking to see if we have a source, and if that source is also valid
	if((_impl->ctx.sourceType == NodeSourceNone) ||
	   (_impl->ctx.sourceType == NodeSourceUnit     && !_impl->ctx.sourceUnit) ||
	   (_impl->ctx.sourceType == NodeSourceCallback && !_impl->ctx.sourceCallback.inputProc))
	{
		std::cout << "DSP Node " << getName()<< " can't be connected without a source" << std::endl;
		AURenderCallbackStruct silentCallback = {SilentRenderCallback};
		destination.setRenderCallback(silentCallback);
		return destination;
	}
	
	_impl->ctx.sourceBus = sourceBus;
	AURenderCallbackStruct callback = {RenderAndCopy, &_impl->ctx};
	destination.setRenderCallback(callback, destinationBus);
	destination.setSourceDSPNode(this);
	
	return destination;
}

// ----------------------------------------------------------
ofxAudioUnitDSPNode& ofxAudioUnitDSPNode::connectTo(ofxAudioUnitDSPNode &destination, int destinationBus, int sourceBus)
// ----------------------------------------------------------
{
	_impl->ctx.sourceBus = sourceBus;
	AURenderCallbackStruct callback = {RenderAndCopy, &_impl->ctx};
	destination.setSource(callback);
	destination.setSourceDSPNode(this);
	
	return destination;
}

// ----------------------------------------------------------
void ofxAudioUnitDSPNode::setSource(ofxAudioUnit * source)
// ----------------------------------------------------------
{
	_impl->ctx.sourceDSPNode = nullptr;
	_impl->ctx.sourceUnit = source;
	_impl->ctx.sourceType = NodeSourceUnit;
	_impl->channelsToBuffer = source->getNumOutputChannels();
	setBufferSize(_impl->samplesToBuffer);
}

// ----------------------------------------------------------
void ofxAudioUnitDSPNode::setSource(AURenderCallbackStruct callback, UInt32 channels)
// ----------------------------------------------------------
{
	_impl->ctx.sourceCallback = callback;
	_impl->ctx.sourceType = NodeSourceCallback;
	_impl->channelsToBuffer = channels;
	setBufferSize(_impl->samplesToBuffer);
}

// ----------------------------------------------------------
AudioStreamBasicDescription ofxAudioUnitDSPNode::getSourceASBD(int sourceBus) const
// ----------------------------------------------------------
{
	AudioStreamBasicDescription ASBD = {0};
	
	if(_impl->ctx.sourceType == NodeSourceUnit && _impl->ctx.sourceUnit != NULL) {
		UInt32 dataSize = sizeof(ASBD);
		OSStatus s = AudioUnitGetProperty(*(_impl->ctx.sourceUnit),
										  kAudioUnitProperty_StreamFormat,
										  kAudioUnitScope_Output,
										  sourceBus,
										  &ASBD,
										  &dataSize);
		if(s != noErr) {
			ASBD = (AudioStreamBasicDescription){0};
		}
	}
	
	return ASBD;
}

#pragma mark - Buffer Size

// ----------------------------------------------------------
void ofxAudioUnitDSPNode::setBufferSize(unsigned int samplesToBuffer)
// ----------------------------------------------------------
{
	_impl->samplesToBuffer = samplesToBuffer;
	_impl->ctx.setCircularBufferSize(_impl->channelsToBuffer, _impl->samplesToBuffer);
}

// ----------------------------------------------------------
unsigned int ofxAudioUnitDSPNode::getBufferSize() const
// ----------------------------------------------------------
{
	return _impl->samplesToBuffer;
}

#pragma mark - Getting Samples


void ofxAudioUnitDSPNode::getSamplesFromChannel(std::vector<Float32> &samples, unsigned int channel) const
{
	if(_impl->ctx.circularBuffers.size() > channel) {
		ExtractSamplesFromCircularBuffer(samples, &_impl->ctx.circularBuffers[channel]);
	} else {
		samples.clear();
	}
}

void ofxAudioUnitDSPNode::setProcessCallback(AURenderCallbackStruct processCallback)
{
	_impl->ctx.bufferMutex.lock();
	_impl->ctx.processCallback = processCallback;
	_impl->ctx.bufferMutex.unlock();
}
ofxAudioUnit * ofxAudioUnitDSPNode::getSourceAU(){
	if(_impl->ctx.sourceType == ofxAudioUnitDSPNode::NodeSourceUnit ){
		return _impl->ctx.sourceUnit;
	}
	return NULL;
}
ofxAudioUnitDSPNode* ofxAudioUnitDSPNode::getSourceDSPNode(){
	if(_impl->ctx.sourceType == ofxAudioUnitDSPNode::NodeSourceCallback) {
		return _impl->ctx.sourceDSPNode;
	}
	return NULL;
}

void ofxAudioUnitDSPNode::setSourceDSPNode(ofxAudioUnitDSPNode* source){
	_impl->ctx.sourceDSPNode = source;
	_impl->ctx.sourceUnit = nullptr;
}


string ofxAudioUnitDSPNode::getName(){
	if(name.empty()){
		return "ofxAudioUnitDSPNode";
	}else{
		return name;
	}
}




#pragma mark - Render callbacks

// ----------------------------------------------------------
OSStatus RenderAndCopy(void * inRefCon,
					   AudioUnitRenderActionFlags * ioActionFlags,
					   const AudioTimeStamp * inTimeStamp,
					   UInt32 inBusNumber,
					   UInt32 inNumberFrames,
					   AudioBufferList * ioData)
{
	ofxAudioUnitDSPNode::DSPNodeContext * ctx = static_cast<ofxAudioUnitDSPNode::DSPNodeContext *>(inRefCon);
	
	OSStatus status;


	
	if(ctx->sourceType == ofxAudioUnitDSPNode::NodeSourceUnit && ctx->sourceUnit->getUnitRef()) {
		status = ctx->sourceUnit->render(ioActionFlags, inTimeStamp, ctx->sourceBus, inNumberFrames, ioData);
	} else if(ctx->sourceType == ofxAudioUnitDSPNode::NodeSourceCallback) {
		status = (ctx->sourceCallback.inputProc)(ctx->sourceCallback.inputProcRefCon,
												 ioActionFlags,
												 inTimeStamp,
												 ctx->sourceBus,
												 inNumberFrames,
												 ioData);
	} else {
		// if we don't have a source, render silence (or else you'll get an extremely loud
		// buzzing noise when we attempt to render a NULL unit. Ow.)
		status = SilentRenderCallback(inRefCon, ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
	}
	
	if(ctx->processCallback.inputProc) {
		(ctx->processCallback.inputProc)(ctx->processCallback.inputProcRefCon,
										 ioActionFlags,
										 inTimeStamp,
										 ctx->sourceBus,
										 inNumberFrames,
										 ioData);
	}
	
	if(ctx->bufferMutex.try_lock()) {
		if(status == noErr) {
			const size_t buffersToCopy = std::min<size_t>(ctx->circularBuffers.size(), ioData->mNumberBuffers);
			
			for(int i = 0; i < buffersToCopy; i++) {
				CopyAudioBufferIntoCircularBuffer(&ctx->circularBuffers[i], ioData->mBuffers[i]);
			}
		}
		ctx->bufferMutex.unlock();
	}
	
	return status;
}

OSStatus SilentRenderCallback(void * inRefCon,
							  AudioUnitRenderActionFlags * ioActionFlags,
							  const AudioTimeStamp * inTimeStamp,
							  UInt32 inBusNumber,
							  UInt32 inNumberFrames,
							  AudioBufferList * ioData)
{
	for(int i = 0; i < ioData->mNumberBuffers; i++) {
		memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[0].mDataByteSize);
	}
	
	*ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
	
	return noErr;
}

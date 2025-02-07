#include "ofxAudioUnit.h"
#include "ofxAudioUnitUtils.h"
#include "ofUtils.h"
#include "ofTypes.h"
#include <iostream>

using namespace std;

// ----------------------------------------------------------
ofxAudioUnit::ofxAudioUnit(AudioComponentDescription description)
: _desc(description)
// ----------------------------------------------------------
{
	initUnit();
}

// ----------------------------------------------------------
ofxAudioUnit::ofxAudioUnit(OSType type,
						   OSType subType,
						   OSType manufacturer)
// ----------------------------------------------------------
{
	_desc.componentType         = type;
	_desc.componentSubType      = subType;
	_desc.componentManufacturer = manufacturer;
	_desc.componentFlags        = 0;
	_desc.componentFlagsMask    = 0;
	initUnit();
};

// ----------------------------------------------------------
ofxAudioUnit::ofxAudioUnit(const ofxAudioUnit &orig)
: _desc(orig._desc)
// ----------------------------------------------------------
{
	initUnit();
}

// ----------------------------------------------------------
ofxAudioUnit& ofxAudioUnit::operator=(const ofxAudioUnit &orig)
// ----------------------------------------------------------
{
	if(this == &orig) return *this;
	
	_desc = orig._desc;
	initUnit();
	
	return *this;
}

// ----------------------------------------------------------
AudioUnitRef ofxAudioUnit::allocUnit(AudioComponentDescription desc)
// ----------------------------------------------------------
{
	AudioComponent component = AudioComponentFindNext(NULL, &_desc);
	if(!component)
	{
		cout << "Couldn't locate component for description: " << StringForDescription(desc) << endl;
		return AudioUnitRef();
	}
	
	ofPtr<AudioUnit> unit((AudioUnit *)malloc(sizeof(AudioUnit)), AudioUnitDeleter);
	OFXAU_PRINT(AudioComponentInstanceNew(component, unit.get()), "creating new unit");
	return unit;
}

// ----------------------------------------------------------
bool ofxAudioUnit::initUnit()
// ----------------------------------------------------------
{
	_unit = allocUnit(_desc);
	if(_unit) {
		OFXAU_RET_BOOL(AudioUnitInitialize(*_unit), "initializing unit");
	} else {
		return false;
	}
}

// ----------------------------------------------------------
void ofxAudioUnit::AudioUnitDeleter(AudioUnit * unit)
// ----------------------------------------------------------
{
	OFXAU_PRINT(AudioUnitUninitialize(*unit),         "uninitializing unit");
	OFXAU_PRINT(AudioComponentInstanceDispose(*unit), "disposing unit");
}

// ----------------------------------------------------------
ofxAudioUnit::~ofxAudioUnit()
// ----------------------------------------------------------
{
	
}

// ----------------------------------------------------------
bool ofxAudioUnit::setup(AudioComponentDescription description)
// ----------------------------------------------------------
{
	_desc = description;
	return initUnit();
}

// ----------------------------------------------------------
bool ofxAudioUnit::setup(OSType type, OSType subType, OSType manufacturer)
// ----------------------------------------------------------
{
	_desc = (AudioComponentDescription){
		.componentType = type,
		.componentSubType = subType,
		.componentManufacturer = manufacturer
	};
	return initUnit();
}

#pragma mark - Parameters

#if !TARGET_OS_IPHONE

// ----------------------------------------------------------
void ofxAudioUnit::setParameter(AudioUnitParameterID parameter,
								AudioUnitScope scope,
								AudioUnitParameterValue value,
								int bus)
// ----------------------------------------------------------
{
	OFXAU_PRINT(AudioUnitSetParameter(*_unit, parameter, scope, bus, value, 0), "setting parameter");
}

// ----------------------------------------------------------
vector<AudioUnitParameterInfo> ofxAudioUnit::getParameterList(bool includeExpert, bool includeReadOnly)
// ----------------------------------------------------------
{
	vector<AudioUnitParameterInfo> paramList;
	
	AUParamInfo info(*_unit, includeExpert, includeReadOnly);
	
	for (int i = 0; i < info.NumParams(); ++i) {
		if (info.GetParamInfo(i)) {
			paramList.push_back(info.GetParamInfo(i)->ParamInfo());
		}
	}
	return paramList;
}

// ----------------------------------------------------------
void ofxAudioUnit::printParameterList(bool includeExpert, bool includeReadOnly)
// ----------------------------------------------------------
{
	vector<AudioUnitParameterInfo> paramList = getParameterList(includeExpert, includeReadOnly);
	
	cout << "\n[id] param name [min : max : default]" << endl;
	
	for(size_t i = 0; i < paramList.size(); ++i) {
		AudioUnitParameterInfo& p = paramList[i];
		
		const size_t bufferSize = 1024;
		char buffer[bufferSize];
		CFStringGetCString(p.cfNameString, buffer, bufferSize, kCFStringEncodingUTF8);
		string paramName(buffer);
		
		cout << "[" << i << "] " << paramName << " [";
		cout << p.minValue << " : " << p.maxValue << " : " << p.defaultValue << "]" << endl;
		
		if(p.flags & kAudioUnitParameterFlag_CFNameRelease) {
			CFRelease(p.cfNameString);
		}
	}
	
	cout << endl;
}

#endif // !TARGET_OS_IPHONE

#pragma mark - Connections

// ----------------------------------------------------------
ofxAudioUnit& ofxAudioUnit::connectTo(ofxAudioUnit &otherUnit, int destinationBus, int sourceBus)
// ----------------------------------------------------------
{
	if(_unit)
	{
		AudioUnitConnection connection;
		connection.sourceAudioUnit    = *_unit;
		connection.sourceOutputNumber = sourceBus;
		connection.destInputNumber    = destinationBus;
		
		OFXAU_PRINT(AudioUnitSetProperty(*(otherUnit._unit),
										 kAudioUnitProperty_MakeConnection,
										 kAudioUnitScope_Input,
										 destinationBus,
										 &connection,
										 sizeof(AudioUnitConnection)),
					"connecting units");
		
		otherUnit.sourceUnit = this;
		otherUnit.sourceDSP = nullptr;
		
	}

	
	return otherUnit;
}

// ----------------------------------------------------------
ofxAudioUnitDSPNode& ofxAudioUnit::connectTo(ofxAudioUnitDSPNode &node)
// ----------------------------------------------------------
{
	node.setSource(this);
	return node;
}
// ----------------------------------------------------------
ofxAudioUnit * ofxAudioUnit::getSourceAU(){
// ----------------------------------------------------------
	return sourceUnit;
}
// ----------------------------------------------------------
ofxAudioUnitDSPNode* ofxAudioUnit::getSourceDSPNode(){
// ----------------------------------------------------------
	return sourceDSP;
}

// ----------------------------------------------------------
void ofxAudioUnit::setSourceDSPNode(ofxAudioUnitDSPNode* source){
// ----------------------------------------------------------
	sourceDSP = source;
	sourceUnit = nullptr;
}
// ----------------------------------------------------------
std::string ofxAudioUnit::getName(){
// ----------------------------------------------------------
	if(name.empty()){
		return "ofxAudioUnit";
	}else{
		return name;
	}
}

// ----------------------------------------------------------
OSStatus ofxAudioUnit::render(AudioUnitRenderActionFlags *ioActionFlags,
							  const AudioTimeStamp *inTimeStamp,
							  UInt32 inOutputBusNumber,
							  UInt32 inNumberFrames,
							  AudioBufferList *ioData)
// ----------------------------------------------------------
{
	return AudioUnitRender(*_unit, ioActionFlags, inTimeStamp,
						   inOutputBusNumber, inNumberFrames, ioData);
}
// ----------------------------------------------------------
AudioStreamBasicDescription ofxAudioUnit::getSourceASBD(int sourceBus) const{
	AudioStreamBasicDescription ASBD = {0};
	
	if(_unit) {
		UInt32 ASBD_size = sizeof(ASBD);
		
		OFXAU_PRINT(AudioUnitGetProperty(*_unit,
										 kAudioUnitProperty_StreamFormat,
										 kAudioUnitScope_Output,
										 0,
										 &ASBD,
										 &ASBD_size),
					"getting unit's output ASBD");
	}
	return ASBD;
}
// ----------------------------------------------------------
UInt32 ofxAudioUnit::getNumOutputChannels() const
// ----------------------------------------------------------
{
	if(!_unit) return 0;
	
	AudioStreamBasicDescription ASBD = {0};
	UInt32 ASBD_size = sizeof(ASBD);
	
	OFXAU_PRINT(AudioUnitGetProperty(*_unit,
									 kAudioUnitProperty_StreamFormat,
									 kAudioUnitScope_Output,
									 0,
									 &ASBD,
									 &ASBD_size),
				"getting unit's output ASBD");
	
	return ASBD.mChannelsPerFrame;
}

#pragma mark - Presets

// ----------------------------------------------------------
CFURLRef CreateURLFromPath(const std::string &path)
// ----------------------------------------------------------
{
	return CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
												   (const UInt8*)path.c_str(),
												   path.length(),
												   NULL);
}

// ----------------------------------------------------------
CFURLRef CreateAbsURLForFileInDataFolder(const std::string &presetName)
// ----------------------------------------------------------
{
	return CreateURLFromPath(ofFilePath::getAbsolutePath(ofToDataPath(presetName)));
}

// ----------------------------------------------------------
std::string StringForPathFromURL(const CFURLRef &urlRef)
// ----------------------------------------------------------
{
	CFStringRef filePath = CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle);
	char buf[PATH_MAX];
	CFStringGetCString(filePath, buf, PATH_MAX, kCFStringEncodingUTF8);
	CFRelease(filePath);
	return std::string(buf);
}

// ----------------------------------------------------------
bool ofxAudioUnit::loadPreset(const CFURLRef &presetURL)
// ----------------------------------------------------------
{
	CFDataRef         presetData;
	CFPropertyListRef presetPList;
	Boolean           presetReadSuccess;
	SInt32            presetReadErrorCode;
	OSStatus          presetSetStatus;
	
	presetReadSuccess = CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault,
																 presetURL,
																 &presetData,
																 NULL,
																 NULL,
																 &presetReadErrorCode);
	if(presetReadSuccess)
	{
		presetPList = CFPropertyListCreateWithData(kCFAllocatorDefault,
												   presetData,
												   kCFPropertyListImmutable,
												   NULL,
												   NULL);
		
		presetSetStatus = AudioUnitSetProperty(*_unit,
											   kAudioUnitProperty_ClassInfo,
											   kAudioUnitScope_Global,
											   0,
											   &presetPList,
											   sizeof(presetPList));
		CFRelease(presetData);
		CFRelease(presetPList);
	}
	else 
	{
		cout << "Couldn't read preset at " << StringForPathFromURL(presetURL) << endl;
	}
	
	bool presetSetSuccess = presetReadSuccess && (presetSetStatus == noErr);
	
#if !TARGET_OS_IPHONE
	if(presetSetSuccess)
	{
		// Notify any listeners that params probably changed
		AudioUnitParameter paramNotification;
		paramNotification.mAudioUnit   = *_unit;
		paramNotification.mParameterID = kAUParameterListener_AnyParameter;
		AUParameterListenerNotify(NULL, NULL, &paramNotification);
	}
#endif
	
	return presetSetSuccess;
}

// ----------------------------------------------------------
bool ofxAudioUnit::savePreset(const CFURLRef &presetURL)
// ----------------------------------------------------------
{
	// getting preset data from AU
	CFPropertyListRef preset;
	UInt32 presetSize = sizeof(preset);
	
	OFXAU_RET_FALSE(AudioUnitGetProperty(*_unit,
										 kAudioUnitProperty_ClassInfo,
										 kAudioUnitScope_Global,
										 0,
										 &preset,
										 &presetSize),
					"getting preset data");
	
	if(!CFPropertyListIsValid(preset, kCFPropertyListXMLFormat_v1_0)) return false;
	
	// if succesful, writing it to a file
	CFDataRef presetData = CFPropertyListCreateXMLData(kCFAllocatorDefault, preset);
	
	ofDirectory dataDir = ofDirectory(ofToDataPath(""));
	if(!dataDir.exists()) dataDir.create();
	
	SInt32 errorCode;
	Boolean writeSuccess = CFURLWriteDataAndPropertiesToResource(presetURL, 
																 presetData,
																 NULL,
																 &errorCode);
	
	CFRelease(presetData);
	
	if(!writeSuccess)
	{
		cout << "Error " << errorCode << " writing preset file at " 
		<< StringForPathFromURL(presetURL) << endl;
	}
	
	return writeSuccess;
}

// ----------------------------------------------------------
bool ofxAudioUnit::loadCustomPreset(const std::string &presetName)
// ----------------------------------------------------------
{
	std::string fileName = std::string(presetName).append(".aupreset");
	CFURLRef URL = CreateAbsURLForFileInDataFolder(fileName);
	bool successful = loadPreset(URL);
	CFRelease(URL);
	return successful;
}

// ----------------------------------------------------------
bool ofxAudioUnit::loadCustomPresetAtPath(const std::string &presetPath)
// ----------------------------------------------------------
{
	CFURLRef URL = CreateURLFromPath(presetPath);
	bool successful = loadPreset(URL);
	CFRelease(URL);
	return successful;
}

// ----------------------------------------------------------
bool ofxAudioUnit::saveCustomPreset(const std::string &presetName)
// ----------------------------------------------------------
{
	std::string fileName = std::string(presetName).append(".aupreset");
	CFURLRef URL = CreateAbsURLForFileInDataFolder(fileName);
	bool successful = savePreset(URL);
	CFRelease(URL);
	return successful;
}

// ----------------------------------------------------------
bool ofxAudioUnit::saveCustomPresetAtPath(const std::string &presetPath)
// ----------------------------------------------------------
{
	CFURLRef URL = CreateURLFromPath(presetPath);
	bool successful = savePreset(URL);
	CFRelease(URL);
	return successful;
}

#pragma mark - Render Callbacks

// ----------------------------------------------------------
void ofxAudioUnit::setRenderCallback(AURenderCallbackStruct callback, int bus)
// ----------------------------------------------------------
{
	OFXAU_PRINT(AudioUnitSetProperty(*_unit,
									 kAudioUnitProperty_SetRenderCallback,
									 kAudioUnitScope_Global,
									 bus,
									 &callback,
									 sizeof(callback)),
				"setting render callback");
}
// ----------------------------------------------------------
void ofxAudioUnit::setBypass(bool bBypass)
// ----------------------------------------------------------
{
   UInt32 value = (bBypass ? 1 : 0);

    OFXAU_PRINT(AudioUnitSetProperty(*_unit,
                                 kAudioUnitProperty_BypassEffect,
                                 kAudioUnitScope_Global,
                                 0,
                                 &value,
                                 sizeof(value)),
                "setting bypass");
}

// ----------------------------------------------------------
bool ofxAudioUnit::isBypassed() const
// ----------------------------------------------------------
{
    
    UInt32 value;
    
    if(_unit) {
        UInt32 valueSize = sizeof(value);
        
        OFXAU_PRINT(AudioUnitGetProperty(*_unit,
                                         kAudioUnitProperty_BypassEffect,
                                         kAudioUnitScope_Output,
                                         0,
                                         &value,
                                         &valueSize),
                    "getting is bypassed");

    
    }
    return (value > 0 );
}

meta:
	ADDON_NAME = ofxAudioUnit
	ADDON_DESCRIPTION = Simple interface for working with Audio Units
	ADDON_AUTHOR = Adam Carlucci
	ADDON_TAGS = "sound" "audio" "audio unit" 
	ADDON_URL = https://github.com/admsyn/ofxAudioUnit

common:
	ADDON_FRAMEWORKS = CoreMIDI
	ADDON_DEFINES = USING_OFX_AUDIO_UNIT
osx:
	ADDON_FRAMEWORKS += CoreAudioKit
	ADDON_FRAMEWORKS += AudioUnit
    

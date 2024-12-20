﻿#include "precomp_ntlsound.h"
#include "NtlSoundManager.h"

#include <crtdbg.h>

// sound
#include "NtlSoundGlobal.h"
#include "NtlChannelGroup.h"
#include "NtlMusicGroup.h"
#include "NtlObjectGroup.h"
#include "NtlWeatherGroup.h"
#include "NtlJingleGroup.h"
#include "NtlWeatherGroup.h"
#include "NtlFadeInOut.h"
#include "NtlSound.h"
#include "NtlSoundSubSystem.h"
#include "NtlFMODSoundPool.h"
#include "NtlBGMGroup.h"
#include "NtlSoundLogic.h"


#define dSET_INVALID_SOUND_HANDLE(phSoundHandle)	\
	if(phSoundHandle)	\
		*phSoundHandle = INVALID_SOUND_HANDLE

CNtlSoundManager::CNtlSoundManager()
:m_pMasterChannelGroup(NULL)
,m_pMasterDSP(NULL)
,m_pSubSystem(NULL)
{
	for(int i = CHANNEL_GROUP_FIRST ; i < NUM_CHANNEL_GROUP ; ++i )
		m_apChannelGroup[i] = NULL;
}

CNtlSoundManager::CNtlSoundManager(const CNtlSoundManager& soundManager)
{

}

CNtlSoundManager::~CNtlSoundManager()
{
	Release();

#ifdef _DEBUG
	_CrtSetDbgFlag( m_iDebugFlag );
#endif
}

CNtlSoundManager* CNtlSoundManager::GetInstance()
{
	static CNtlSoundManager soundManager;
	return &soundManager;
}

RwBool CNtlSoundManager::IsUsableSound()
{
	if( CNtlSoundGlobal::m_pFMODSystem == NULL)
		return FALSE;

	return TRUE;
}

void CNtlSoundManager::Init(const char* pcPath, float fMasterVolume /* = 1.0 */, float fDopplerScale /* = 1.0 */,
							float fDistacneFactor /* = 1.0 */, float fRollOffScale /* = 1.0 */)
{
	NTL_FUNCTION("CNtlSoundManager::Create");
	
	FMOD_RESULT result;

#ifdef _DEBUG
	m_iDebugFlag =_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// »ç¿îµå ÆÄÀÏÀÇ °æ·Î ÁöÁ¤
	CNtlSoundGlobal::m_strFilePath = pcPath;


	////////////////////////////////////////////////////////////////////////////////////////////////////////
	// FMOD EX System create
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	result = FMOD::System_Create(&CNtlSoundGlobal::m_pFMODSystem);
	if( result != FMOD_OK )
	{
		Logic_NtlSoundLog("CNtlSoundManager::Init, FMOD Ex system fail to create", result);
		Release();
		NTL_RETURNVOID();
	}	


	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	Version check
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	unsigned int uiVersion;
	CNtlSoundGlobal::m_pFMODSystem->getVersion(&uiVersion);
	if (uiVersion < FMOD_VERSION)
	{
		char acCondition[128] = "";
		sprintf_s(acCondition, "Error!  You are using an old version of FMOD %08x.  This program requires %08x\n", uiVersion, FMOD_VERSION);

		Logic_NtlSoundLog(acCondition);

		Release();
		NTL_RETURNVOID();
	}

	result = CNtlSoundGlobal::m_pFMODSystem->init(MAX_DBO_CHANNELS, FMOD_INIT_NORMAL, NULL);
	if (result != FMOD_OK)
	{
		Logic_NtlSoundLog("CNtlSoundManager::Init, fail to init FMOD system", result);
		Release();
		NTL_RETURNVOID();
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 0.1 second streaming buffer size
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	result = CNtlSoundGlobal::m_pFMODSystem->setStreamBufferSize(100, FMOD_TIMEUNIT_MS);
	Logic_NtlSoundLog("CNtlSoundManager::Init, fail to set stream buffer size", result);

	////////////////////////////////////////////////////////////////////////////////////////////////////////
	// FMOD system 3d setting
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	result = CNtlSoundGlobal::m_pFMODSystem->set3DSettings(fDopplerScale, fDistacneFactor, fRollOffScale);
	Logic_NtlSoundLog("CNtlSoundManager::Init, fail to set 3D setting", result);


	////////////////////////////////////////////////////////////////////////////////////////////////////////
	// FMOD::SystemÀ¸·Î ºÎÅÍ MasterChannelGroup ¾ò¾î¿À±â
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	result = CNtlSoundGlobal::m_pFMODSystem->getMasterChannelGroup(&m_pMasterChannelGroup);
	if( result != FMOD_OK )
	{
		Logic_NtlSoundLog("CNtlSoundManager::Init, fail to get master channel group", result);
		Release();
		NTL_RETURNVOID();
	}

	CNtlSoundGlobal::m_tMasterVolume.fMainVolume = Logic_GetFMODValidVolume(fMasterVolume);
	m_pMasterChannelGroup->setVolume(CNtlSoundGlobal::m_tMasterVolume.fMainVolume);


	////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create sub system
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	m_pSubSystem = NTL_NEW CNtlSoundSubSystem;
	if( !m_pSubSystem->Create() )
	{
		Logic_NtlSoundLog("CNtlSoundManager::Init, fail to create sub system");
		Release();
		NTL_RETURNVOID();
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Channel Group Creation
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	CreateChannelGroups();
}

void CNtlSoundManager::CreateChannelGroups()
{
	FMOD::ChannelGroup *m_pFMODChannelGroup;

	// so that changing the order of eChannelGroupType in NtlSoundDefines.h will not affect
	// create a class against each index Õë¶ÔÃ¿¸öË÷Òý´´½¨Ò»¸öÀà
	for( RwUInt8 i = CHANNEL_GROUP_FIRST ; i < NUM_CHANNEL_GROUP ; ++i )
	{
		if( i == CHANNEL_GROUP_UI_SOUND )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlChannelGroup(CHANNEL_GROUP_UI_SOUND);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_UI_SOUND", &m_pFMODChannelGroup);	// »õ·Î¿î FMOD::ChannelGroup À» »ý¼ºÇÑ´Ù
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);				// MasterChannelÀÇ subGroupÀ¸·Î µî·Ï
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 0);				// CNtlChannelGroup ÃÊ±âÈ­
		}
		else if( i == CHANNEL_GROUP_JINGLE_MUSIC )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlJingleGroup(CHANNEL_GROUP_JINGLE_MUSIC);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_JINGLE_MUSIC", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 0);
		}
		else if( i == CHANNEL_GROUP_FLASH_MUSIC )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlMusicGroup(CHANNEL_GROUP_FLASH_MUSIC);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_FLASH_MUSIC", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 1);
		}
		else if( i == CHANNEL_GROUP_BGM )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlBGMGroup(CHANNEL_GROUP_BGM);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_BGM", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 1);
		}
		else if( i == CHANNEL_GROUP_AVATAR_VOICE_SOUND )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlChannelGroup(CHANNEL_GROUP_AVATAR_VOICE_SOUND);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_AVATAR_VOICE_SOUND", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 3);
		}
		else if( i == CHANNEL_GROUP_AVATAR_EFFECT_SOUND )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlChannelGroup(CHANNEL_GROUP_AVATAR_EFFECT_SOUND);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_AVATAR_EFFECT_SOUND", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 3);
		}
		else if( i == CHANNEL_GROUP_VOICE_SOUND )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlChannelGroup(CHANNEL_GROUP_VOICE_SOUND);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_VOICE_SOUND", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 3);
		}
		else if( i == CHANNEL_GROUP_EFFECT_SOUND )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlChannelGroup(CHANNEL_GROUP_EFFECT_SOUND);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_EFFECT_SOUND", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 3);
		}
		else if( i == CHANNEL_GROUP_OBJECT_MUSIC )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlObjectGroup(CHANNEL_GROUP_OBJECT_MUSIC);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_OBJECT_MUSIC", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup);
		}
		else if( i == CHANNEL_GROUP_AMBIENT_MUSIC )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlMusicGroup(CHANNEL_GROUP_AMBIENT_MUSIC);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_AMBIENT_MUSIC", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 1);
		}
		else if( i == CHANNEL_GROUP_WEATHER_EFFECT_SOUND )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlChannelGroup(CHANNEL_GROUP_WEATHER_EFFECT_SOUND);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_WEATHER_EFFECT_SOUND", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 0);
		}
		else if( i == CHANNEL_GROUP_WEATHER_MUSIC )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlWeatherGroup(CHANNEL_GROUP_WEATHER_MUSIC);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_WEATHER_MUSIC", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 1);
		}
		else if( i == CHANNEL_GROUP_JINGLE_MUSIC_WITHOUT_FADE )
		{
			m_apChannelGroup[i] = NTL_NEW CNtlChannelGroup(CHANNEL_GROUP_JINGLE_MUSIC_WITHOUT_FADE);
			CNtlSoundGlobal::m_pFMODSystem->createChannelGroup("CHANNEL_GROUP_JINGLE_MUSIC_WITHOUT_FADE", &m_pFMODChannelGroup);
			m_pMasterChannelGroup->addGroup(m_pFMODChannelGroup);
			m_apChannelGroup[i]->Create(m_pFMODChannelGroup, 0);
		}
	}
}

void CNtlSoundManager::Release()
{
	NTL_FUNCTION("CNtlSoundManager::Release");	

	if( m_pMasterDSP )
	{
		NTL_DELETE(m_pMasterDSP);
	}

	for(int i = CHANNEL_GROUP_FIRST ; i < NUM_CHANNEL_GROUP ; ++i )
	{
		if( m_apChannelGroup[i] )
		{
			m_apChannelGroup[i]->Destory();
			NTL_DELETE(m_apChannelGroup[i]);
		}
	}

	if( m_pSubSystem )
	{
		m_pSubSystem->Destroy();
		NTL_DELETE(m_pSubSystem);
	}

	if(m_pMasterChannelGroup)
	{
		m_pMasterChannelGroup->release();
		m_pMasterChannelGroup = NULL;
	}

	CNtlSound::Shutdown();

	if(CNtlSoundGlobal::m_pFMODSystem)
	{
		CNtlSoundGlobal::m_pFMODSystem->close();
		CNtlSoundGlobal::m_pFMODSystem->release();
		CNtlSoundGlobal::m_pFMODSystem = NULL;
	}	

	NTL_RETURNVOID();
}

void CNtlSoundManager::Reset()
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	for(int i = 0 ; i < NUM_CHANNEL_GROUP ; ++i)
		m_apChannelGroup[i]->Reset();
}

void CNtlSoundManager::SetListenerPosition(float fXPos, float fYPos, float fZPos)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	FMOD_VECTOR listenerPos = { dCONVERT_COORDINATE_X(fXPos), fYPos, fZPos };
	CNtlSoundGlobal::m_pFMODSystem->set3DListenerAttributes(0, &listenerPos, 0, 0, 0);
}

FMOD_VECTOR NormalizeVector(const FMOD_VECTOR& vec) {
	float length = sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
	FMOD_VECTOR normalizedVec = { vec.x / length, vec.y / length, vec.z / length };
	return normalizedVec;
}

void CNtlSoundManager::SetListenerPosition(float fXPos, float fYPos, float fZPos, 
										   float fXFoward, float fYFoward, float fZFoward, 
										   float fXUp, float fYUp, float fZUp)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	FMOD_VECTOR listenerPos		= { dCONVERT_COORDINATE_X(fXPos), fYPos, fZPos };
	FMOD_VECTOR listenerFoward	= { dCONVERT_COORDINATE_X(fXFoward), fYFoward, fZFoward };
	FMOD_VECTOR listenerUp		= { dCONVERT_COORDINATE_X(fXUp), fYUp, fZUp };

	// Normalize the forward and up vectors
	listenerFoward = NormalizeVector(listenerFoward);
	listenerUp = NormalizeVector(listenerUp);

	CNtlSoundGlobal::m_pFMODSystem->set3DListenerAttributes(0, &listenerPos, 0, &listenerFoward, &listenerUp);
}

int CNtlSoundManager::Play(sNtlSoundPlayParameta* pParameta)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return SOUNDRESULT_OK;

	/*
	// When you take a play position log
	char pcResult[256] = "";
	sprintf_s(pcResult, "Group : %s, File : %s, X : %f, Y : %f, Z : %f, Volmue : %f, mix : %f, max : %f\n",
	GetChannelGroupName(iChannelGroup), pcName, fXPos, fYPos, fZPos, fVolume, fMinDistance, fMaxDistance);
	NtlLogFilePrint(pcResult);*/

	if( IsProhibition(pParameta->iChannelGroup) )
		return SOUNDRESULT_PROHIBITION_STATE;

	if( pParameta->iChannelGroup == CHANNEL_GROUP_FLASH_MUSIC )
		return SOUNDRESULT_DISABLE_FLASH_MUSIC;

	FMOD_RESULT		result;

	// Check if you can play a new sound
	int iResult = CanPlay(pParameta);
	//DBO_WARNING_MESSAGE("CanPlay: iResult: " << iResult);
	if( iResult == SOUNDRESULT_RECOVERY_FROM_FADE_OUT )
	{
		CNtlWeatherGroup* pWeatherGroup = reinterpret_cast<CNtlWeatherGroup*>( m_apChannelGroup[CHANNEL_GROUP_WEATHER_MUSIC] );
		pParameta->hHandle = pWeatherGroup->GetRecoverySoundHandle();

		return SOUNDRESULT_OK;
	}
	else if( iResult != SOUNDRESULT_OK )
		return iResult;

	// Adjust sound attenuation range
	if( pParameta->fMinDistance + MIN_DISTANCE_BETWEEN_MINMAX >= pParameta->fMaxDistance )
	{
		Logic_NtlSoundLog("CNtlSoundManager::Play, The min distance is longer far than max distacne", pParameta->iChannelGroup, pParameta->pcFileName);
		pParameta->fMinDistance = pParameta->fMaxDistance - MIN_DISTANCE_BETWEEN_MINMAX;
	}

	std::string fullName = CNtlSoundGlobal::m_strFilePath + pParameta->pcFileName;


	///////////////////////////////////////////////////////////////////////////////////////////////////////
	// Sound µ¥ÀÌÅÍ ÃÊ±âÈ­
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	CNtlSound* pSound = NTL_NEW CNtlSound(Logic_GetNewSoundHandle(), pParameta->pcFileName );
	pSound->m_iChannelGroup				= pParameta->iChannelGroup;
	pSound->m_eState					= SPS_PLAYING;
	pSound->m_bLoop						= pParameta->bLoop ;
	pSound->m_tVolume.fMainVolume		= Logic_GetFMODValidVolume(pParameta->fVolume);
	pSound->m_tVolume.fFadeVolume = 1.f;
	pSound->m_tVolume.fWhenMoviePlayVolume = 1.f;
	pSound->m_fXPos						= pParameta->fXPos;
	pSound->m_fYPos						= pParameta->fYPos;
	pSound->m_fZPos						= pParameta->fZPos;
	pSound->m_fMinDistance				= pParameta->fMinDistance;
	pSound->m_fMaxDistance				= pParameta->fMaxDistance;

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	// Sound Call in
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	switch(pParameta->iChannelGroup)
	{
		case CHANNEL_GROUP_UI_SOUND:
		case CHANNEL_GROUP_WEATHER_EFFECT_SOUND:
		case CHANNEL_GROUP_JINGLE_MUSIC_WITHOUT_FADE:
		{
			result = API_Create_Sound(pSound, pParameta, d2D_SOUND_MODE, fullName);
		}
		break;
		case CHANNEL_GROUP_JINGLE_MUSIC:
		case CHANNEL_GROUP_FLASH_MUSIC:
		case CHANNEL_GROUP_BGM:
		case CHANNEL_GROUP_AMBIENT_MUSIC:
		case CHANNEL_GROUP_WEATHER_MUSIC:
		{
			result = API_Create_Stream(pSound, pParameta, d2D_SOUND_MODE, fullName);
		}
		break;
		case CHANNEL_GROUP_AVATAR_VOICE_SOUND:
		case CHANNEL_GROUP_AVATAR_EFFECT_SOUND:
		case CHANNEL_GROUP_VOICE_SOUND:
		case CHANNEL_GROUP_EFFECT_SOUND:
		{
			unsigned int iCount = 0;

			iCount += m_apChannelGroup[CHANNEL_GROUP_AVATAR_VOICE_SOUND]->GetPlayingChannels();
			iCount += m_apChannelGroup[CHANNEL_GROUP_AVATAR_EFFECT_SOUND]->GetPlayingChannels();
			iCount += m_apChannelGroup[CHANNEL_GROUP_VOICE_SOUND]->GetPlayingChannels();
			iCount += m_apChannelGroup[CHANNEL_GROUP_EFFECT_SOUND]->GetPlayingChannels();
			//DBO_WARNING_MESSAGE("EFFECT SOUNDS: " << iCount);
			if( iCount >= MAX_EFFECT_CHANNELS )
			{
				pSound->Release();
				return SOUNDRESULT_MAX_EFFECT_CHANNELS;
			}

			result = API_Create_Sound(pSound, pParameta, d3D_SOUND_MODE, fullName);
		}
		break;
		case CHANNEL_GROUP_OBJECT_MUSIC:
		{
			CNtlObjectGroup* pEnvironmentGroup = reinterpret_cast<CNtlObjectGroup*>(m_apChannelGroup[pParameta->iChannelGroup]);
			float fL = LengthFromListenerToSound(pParameta->fXPos, pParameta->fYPos, pParameta->fZPos);
			//DBO_WARNING_MESSAGE("fL: " << fL << ", pParameta->fMaxDistance: " << pParameta->fMaxDistance);
			if(fL > pParameta->fMaxDistance )
			{
				if( Logic_IsExistFile(fullName.c_str()) == false )
				{
					Logic_NtlSoundLog("CNtlSoundManager::Play, Not exist sound file", CHANNEL_GROUP_OBJECT_MUSIC, pSound->m_strName.c_str());
					pSound->Release();
					return SOUNDRESULT_NOT_EXIST_FILE;
				}
				
				pEnvironmentGroup->StoreSleepingSound(pSound);
				
				pParameta->hHandle = pSound->m_hHandle;

				return SOUNDRESULT_OK;
			}

			result = API_Create_Stream(pSound, pParameta, d3D_SOUND_MODE, fullName);
		}
		break;
		default:
		{
			Logic_NtlSoundLog("CNtlSoundManager::Play, Invalid channel group", pSound->m_strName.c_str());
			pSound->Release();
			return SOUNDRESULT_INVALID_CHANNELGROUP;
		}
		break;
	}
	//DBO_WARNING_MESSAGE("CREATE SOUND RESULT: " << result);
	if( result != FMOD_OK )
	{
		pSound->FreeMemoryData();
		Logic_NtlSoundLog("CNtlSoundManager::Play, fail to create sound or stream", result, pParameta->pcFileName);
		pSound->Release();
		return SOUNDRESULT_NOT_CREATED_SOUND;
	}
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	// Play Sound
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	// Do not play sound until the third argument is set to true to finish setting all sounds.
	result = CNtlSoundGlobal::m_pFMODSystem->playSound(pSound->m_pFMODSound, 0, true, &(pSound->m_pFMODChannel) );
	//DBO_WARNING_MESSAGE("PLAY SOUND RESULT: " << result);
	if( result != FMOD_OK )
	{
		pSound->FreeMemoryData();
		Logic_NtlSoundLog("CNtlSoundManager::Play, fail to play sound", result, pSound->m_strName.c_str());
		pSound->Release();
		return SOUNDRESULT_FAIL_PLAY;
	}

	switch(pSound->m_iChannelGroup)
	{
			case CHANNEL_GROUP_AVATAR_VOICE_SOUND:
			case CHANNEL_GROUP_AVATAR_EFFECT_SOUND:
			case CHANNEL_GROUP_VOICE_SOUND:
			case CHANNEL_GROUP_EFFECT_SOUND:
			case CHANNEL_GROUP_OBJECT_MUSIC:
			{
				FMOD_VECTOR pos = { dCONVERT_COORDINATE_X(pSound->m_fXPos), pSound->m_fYPos, pSound->m_fZPos};
				FMOD_VECTOR vel = { dCONVERT_COORDINATE_X(0.0f), 0.0f, 0.0f };

				pSound->m_pFMODChannel->set3DAttributes(&pos, &vel);
				pSound->SetMinMax(pSound->m_fMinDistance, pSound->m_fMaxDistance);
			}
			break;
	}
	
	pSound->m_pFMODChannel->setVolume( Logic_CalcPlayVolume(&pSound->m_tVolume) );
	//DBO_WARNING_MESSAGE("Volume: " << Logic_CalcPlayVolume(&pSound->m_tVolume) << ", pSound->m_hHandle: " << pSound->m_hHandle);

	eStoreResult EStoreResult = m_apChannelGroup[pParameta->iChannelGroup]->StoreSound(pSound, pParameta);

	switch( EStoreResult )
	{
		case STORE_READY_TO_PLAY:
		{
			// I actually play the sound.
			pSound->m_pFMODChannel->setPaused(false);
		}
		break;
		case STORE_LIST:
		{
			pParameta->hHandle = pSound->m_hHandle;
			pSound->Release();
			return SOUNDRESULT_OK;
		}
		break;
		case STORE_FAIL:
		{
			pSound->Release();
			return SOUNDRESULT_FAILT_STORE_SOUND;
		}
		break;
		default:
		{
			NTL_ASSERT(false, "CNtlSoundManager::Play, Invalid eStoreResult type");
		}
		break;
	}


	// Handle returned
	pParameta->hHandle = pSound->m_hHandle;
	
	CNtlSoundGlobal::m_setPlayingSoundHandle.insert(pParameta->hHandle);

	return SOUNDRESULT_OK;
}

int CNtlSoundManager::ReplayEnvironmentSound(CNtlSound* pSound)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return SOUNDRESULT_OK;
	/*
	// ÇÃ·¹ÀÌ À§Ä¡ ·Î±×¸¦ ÂïÀ» ¶§
	char pcResult[256] = "";
	sprintf_s(pcResult, "Group : %s, File : %s, X : %f, Y : %f, Z : %f, Volmue : %f, mix : %f, max : %f\n",
	GetChannelGroupName(iChannelGroup), pcName, fXPos, fYPos, fZPos, fVolume, fMinDistance, fMaxDistance);
	NtlLogFilePrint(pcResult);
	*/

	FMOD_RESULT		result;

	// »õ·Î¿î »ç¿îµå¸¦ ÇÃ·¹ÀÌ ÇÒ ¼ö ÀÖ´Â »óÈ²ÀÎÁö Ã¼Å©
	int iResult = CanPlay(CHANNEL_GROUP_OBJECT_MUSIC, pSound->m_strName.c_str(), pSound->m_fXPos, pSound->m_fYPos, pSound->m_fZPos);
	if( iResult != SOUNDRESULT_OK )
	{
		return iResult;
	}

	std::string fullName = CNtlSoundGlobal::m_strFilePath + pSound->m_strName;


	///////////////////////////////////////////////////////////////////////////////////////////////////////
	// Sound¸¦ ºÒ·¯µéÀÎ´Ù
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	CNtlObjectGroup* pEnvironmentGroup = reinterpret_cast<CNtlObjectGroup*>(m_apChannelGroup[CHANNEL_GROUP_OBJECT_MUSIC]);
	if (LengthFromListenerToSound(pSound->m_fXPos, pSound->m_fYPos, pSound->m_fZPos) > pSound->m_fMaxDistance)
		return SOUNDRESULT_OK;


	FMOD_MODE mode = d3D_SOUND_MODE;

	///< Sound ¹Ýº¹ ¿¬ÁÖ
	if( pSound->m_bLoop )				
		mode |= FMOD_LOOP_NORMAL;

	if( g_fnCallback_LoadSound_from_Memory )
	{
		void* pData = NULL;
		FMOD_CREATESOUNDEXINFO exinfo;
		memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
		exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);

		(*g_fnCallback_LoadSound_from_Memory)(fullName.c_str(), (void**)&pSound->pMemoryData, (int*)&exinfo.length);

		if( pSound->pMemoryData )
		{
			mode |= FMOD_OPENMEMORY;
			result = CNtlSoundGlobal::m_pFMODSystem->createStream((const char*)pSound->pMemoryData, mode, &exinfo, &(pSound->m_pFMODSound) );
		}
		else
		{
			result = FMOD_ERR_MEMORY_CANTPOINT;
		}
	}
	else
	{
		result = CNtlSoundGlobal::m_pFMODSystem->createStream(fullName.c_str(), mode, 0, &(pSound->m_pFMODSound) );
	}


	if( result != FMOD_OK )
	{
		pSound->FreeMemoryData();
		Logic_NtlSoundLog("CNtlSoundManager::ReplayEnvironmentSound, fail to create environment sound", result, pSound->m_strName.c_str());
		return SOUNDRESULT_NOT_CREATED_SOUND;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	// Sound¸¦ ¿¬ÁÖÇÑ´Ù
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	// 3¹øÂ° ÀÎÀÚ¸¦ true¸¦ ÁÖ¾î ¸ðµç »ç¿îµåÀÇ ¼¼ÆÃÀÌ ³¡³¯ ¶§ ±îÁö »ç¿îµå ÇÃ·¹ÀÌ¸¦ ÇÏÁö ¾Ê´Â´Ù.
	result = CNtlSoundGlobal::m_pFMODSystem->playSound(pSound->m_pFMODSound, 0, true, &(pSound->m_pFMODChannel) );
	if( result != FMOD_OK )
	{
		pSound->FreeMemoryData();
		Logic_NtlSoundLog("CNtlSoundManager::ReplayEnvironmentSound, fail to play environment sound", result);
		return SOUNDRESULT_FAIL_PLAY;
	}	

	FMOD_VECTOR pos = { dCONVERT_COORDINATE_X(pSound->m_fXPos), pSound->m_fYPos, pSound->m_fZPos };
	FMOD_VECTOR vel = { dCONVERT_COORDINATE_X(0.0f), 0.0f, 0.0f };

	pSound->m_pFMODChannel->set3DAttributes(&pos, &vel);
	pSound->m_pFMODChannel->setVolume( Logic_CalcPlayVolume( &(pSound->m_tVolume) ) );
	pSound->SetMinMax(pSound->m_fMinDistance, pSound->m_fMaxDistance);


	// ½ÇÁ¦·Î »ç¿îµå¸¦ ÇÃ·¹ÀÌ ÇÑ´Ù.
	pSound->m_pFMODChannel->setPaused(false);


	// soundInfo¿¡ ºÎ°¡Á¤º¸ ÀÔ·Â
	pSound->m_iChannelGroup = CHANNEL_GROUP_OBJECT_MUSIC;
	pSound->m_strName = pSound->m_strName;


	// Channel Group¿¡ ¿¬ÁÖÁ¤º¸¸¦ ÀúÀåÇÑ´Ù
	eStoreResult EStoreResult = m_apChannelGroup[CHANNEL_GROUP_OBJECT_MUSIC]->StoreSound(pSound, NULL);
	switch( EStoreResult )
	{
		case STORE_READY_TO_PLAY:
		{
			// ½ÇÁ¦·Î »ç¿îµå¸¦ ÇÃ·¹ÀÌ ÇÑ´Ù.
			pSound->m_pFMODChannel->setPaused(false);
			break;
		}
		case STORE_LIST:
		{
			pSound->Release();
			return SOUNDRESULT_OK;
			break;
		}
		case STORE_FAIL:
		{
			pSound->Release();
			return SOUNDRESULT_FAILT_STORE_SOUND;
		}
		default:
		{
			NTL_ASSERT(false, "CNtlSoundManager::ReplayEnvironmentSound, Invalid eStoreResult type");
			break;
		}
	}

	pSound->m_eState = SPS_PLAYING;

	CNtlSoundGlobal::m_setPlayingSoundHandle.insert(pSound->m_hHandle);

	return SOUNDRESULT_OK;
}

int CNtlSoundManager::Replay(CNtlSound* pSound)
{
	if(!pSound)
		return SOUNDRESULT_INVALID_HANDLE_PTR;

	std::string fullName = CNtlSoundGlobal::m_strFilePath + pSound->m_strName;
	
	FMOD_RESULT result = CNtlSoundGlobal::m_pFMODSystem->playSound(pSound->m_pFMODSound, 0, false, &(pSound->m_pFMODChannel) );

	if( result != FMOD_OK )
	{			
		Logic_NtlSoundLog("CNtlSoundManager::Replay, fail to play", result, (const char*)pSound->m_strName.c_str());
		pSound->Release();
		return SOUNDRESULT_FAIL_PLAY;
	}

	CNtlSoundGlobal::m_setPlayingSoundHandle.insert(pSound->m_hHandle);

	return SOUNDRESULT_OK;
}

void CNtlSoundManager::Stop(SOUND_HANDLE& rHandle)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
	{
		rHandle = INVALID_SOUND_HANDLE;
		return;
	}

	if( rHandle == INVALID_SOUND_HANDLE )
		return;

	for(int i = 0 ; i < NUM_CHANNEL_GROUP ; ++i )
	{
		if( m_apChannelGroup[i]->Stop(rHandle) )
			break;
	}

	// »ç¿îµå ÇÚµéÀÌ °¡Áö°í ÀÖ´Â sNtlSoundÀÇ µ¥ÀÌÅÍÀÇ ¹«°á¼ºÀº º¸ÀåÇÏÁö ¾Ê´Â´Ù.
	// °¡Áö°í ÀÖ´Â »ç¿îµå ÇÚµé¿¡ ÇØ´çÇÏ´Â sNtlSoundÀÇ ÇÃ·¹ÀÌ°¡ ³¡³ª¼­ µ¥ÀÌÅÍ°¡ Á¸ÀçÇÏÁö ¾ÊÀ» ¼öµµ ÀÖ´Ù.
	// ¾îÂ¶µç StopÀ» È£ÃâÇÏ¸é INVALID_SOUND_HANDLEÀ» ³Ö¾îÁÖÀÚ
	rHandle = INVALID_SOUND_HANDLE;
}

void CNtlSoundManager::StopGroup(int iChannelGroup)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	m_apChannelGroup[iChannelGroup]->StopGroup();
}

int CNtlSoundManager::SetSoundPosition(SOUND_HANDLE hHandle, float fPosX, float fPosY, float fPosZ)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return 0;

	CNtlSound* pSound = GetSound(hHandle);
	if(pSound)
	{
		// If object music is stopped playing, let it play again
		if( pSound->m_iChannelGroup == CHANNEL_GROUP_OBJECT_MUSIC &&
			pSound->m_eState == SPS_SLEEP )
		{
			pSound->m_fXPos = fPosX;
			pSound->m_fYPos = fPosY;
			pSound->m_fZPos = fPosZ;

			ReplayEnvironmentSound(pSound);
		}
		else
		{
			if( pSound->m_pFMODChannel )
			{
				FMOD_VECTOR pos = { dCONVERT_COORDINATE_X(fPosX), fPosY, fPosZ };
				FMOD_VECTOR vel = { dCONVERT_COORDINATE_X(0.0f), 0.0f, 0.0f };
				pSound->m_pFMODChannel->set3DAttributes(&pos, &vel);

				return SOUNDRESULT_OK;
			}
			else
			{
				assert(false);
				return SOUNDRESULT_INVALID_HANDLE_PTR;
			}
		}
	}

	return SOUNDRESULT_INVALID_HANDLE_PTR;
}

void CNtlSoundManager::SetMinMaxDistance(SOUND_HANDLE hHandle, float fMinDistance, float fMaxDistance)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	CNtlSound* pSound = GetSound(hHandle);
	if( !pSound )
		return;

	pSound->SetMinMax(fMinDistance, fMaxDistance);
}

void CNtlSoundManager::GetMinMaxDistance(SOUND_HANDLE hHandle, float &fMinDistance, float &fMaxDistance)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
	{
		fMinDistance = -1;
		fMaxDistance = -1;
		return;
	}

	CNtlSound* pSound = GetSound(hHandle);
	if( !pSound )
		return;

	fMinDistance = pSound->m_tMinMax.fMin * CNtlSoundGlobal::m_fMinMaxRate;
	fMaxDistance = pSound->m_tMinMax.fMax * CNtlSoundGlobal::m_fMinMaxRate;
}

void CNtlSoundManager::Update(float fElapsed)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	// Inside the update of channel, things like 3D sound,
// virtual channel updates, and emulated voice updates are executed.
	CNtlSoundGlobal::m_pFMODSystem->update();
	
	int iBeforeChannelCount = 0, iAfterChannelCount = 0;
	CNtlSoundGlobal::m_pFMODSystem->getChannelsPlaying(&iBeforeChannelCount);

	// °¢ Ã¤³Î Update
	for( int i = CHANNEL_GROUP_FIRST ; i < NUM_CHANNEL_GROUP ; ++i )
	{
		m_apChannelGroup[i]->Update(fElapsed);
	}

	CNtlSoundGlobal::m_pFMODSystem->getChannelsPlaying(&iAfterChannelCount);

	// If there is a channel released after each channel update, play the channel that was terminated.
	if( iBeforeChannelCount > iAfterChannelCount )
	{	
		for( int i = 0 ; i < iBeforeChannelCount - iAfterChannelCount ; ++i )
		{
			CNtlSound* pSound = ((CNtlObjectGroup*)m_apChannelGroup[CHANNEL_GROUP_OBJECT_MUSIC])->GetReleasedSoundbyPriority();
			if( pSound )
			{
				int iResult = ReplayEnvironmentSound(pSound);

				if( iResult == SOUNDRESULT_OK )
				{
					((CNtlObjectGroup*)m_apChannelGroup[CHANNEL_GROUP_OBJECT_MUSIC])->SuccessRelay();
				}
				else
				{
					break;
				}

			}
		}				
	}

	// Environment Group Post Update
	FMOD_VECTOR listenerPos;
	CNtlObjectGroup* pEnvironmentGroup = reinterpret_cast<CNtlObjectGroup*>(m_apChannelGroup[CHANNEL_GROUP_OBJECT_MUSIC]);

	CNtlSoundGlobal::m_pFMODSystem->get3DListenerAttributes(0, &listenerPos, 0, 0, 0);

	listenerPos.x = dCONVERT_COORDINATE_X(listenerPos.x);
	pEnvironmentGroup->PostUpdate(listenerPos.x, listenerPos.y, listenerPos.z);

	//Fade In/Out Update
	//GetFadeInOut()->Update();
}

void CNtlSoundManager::SetValidGroupRange(int iChannelGroup, float fRange)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	m_apChannelGroup[iChannelGroup]->SetValidGroupRange(fRange);
}

float CNtlSoundManager::GetValidGroupRange(int iChannelGroup)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return 0.f;

	return m_apChannelGroup[iChannelGroup]->GetValidGroupRange();
}

const char* CNtlSoundManager::GetSoundName(SOUND_HANDLE hHandle)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return NULL;

	for( int i = CHANNEL_GROUP_FIRST ; i < NUM_CHANNEL_GROUP ; ++i )
	{
		// ¾îÂ÷ÇÇ ¾î¶² Ã¤³Î ±×·ìÀÎÁö¸¦ ¾Ë¾Æ¾ß ÇÏ±â¿¡ °¢°¢ÀÇ Ã¤³Î ±×·ìÀ» °Ë»öÇØ¾ß ÇÑ´Ù
		CNtlSound* pSound = m_apChannelGroup[i]->GetSound(hHandle);
		if(pSound)
		{
			return pSound->m_strName.c_str();
		}
	}

	return NULL;
}

void CNtlSoundManager::SetMasterVolume(float fVolume)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	if( !m_pMasterChannelGroup )
		return;
	
	CNtlSoundGlobal::m_tMasterVolume.fMainVolume = Logic_GetFMODValidVolume(fVolume);

	float fResultVolume = Logic_CalcPlayVolume(&CNtlSoundGlobal::m_tMasterVolume);
	//DBO_WARNING_MESSAGE("SetMasterVolume: fVolume=" << fVolume << ", fResultVolume=" << fResultVolume);
	m_pMasterChannelGroup->setVolume(fResultVolume);
}

float CNtlSoundManager::GetMasterVolume()
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return -1;
	
	return CNtlSoundGlobal::m_tMasterVolume.fMainVolume;
}

void CNtlSoundManager::SetMasterEffect(FMOD_DSP_TYPE eType)
{
	if (!CNtlSoundGlobal::m_pFMODSystem)
		return;

	if (m_mapMasterDSP.find(eType) != m_mapMasterDSP.end())
		return;

	FMOD::DSP* pDSP = nullptr;
	FMOD_RESULT result = CNtlSoundGlobal::m_pFMODSystem->createDSPByType(eType, &pDSP);
	if (result == FMOD_OK && pDSP)
	{
		m_pMasterChannelGroup->addDSP(0, pDSP);
		pDSP->setActive(true);
		m_mapMasterDSP[eType] = pDSP;
	}
}

void CNtlSoundManager::ReleaseMasterEffect(FMOD_DSP_TYPE eType)
{
	auto it = m_mapMasterDSP.find(eType);
	if (it != m_mapMasterDSP.end())
	{
		FMOD::DSP* pDSP = it->second;
		pDSP->setActive(false);
		m_pMasterChannelGroup->removeDSP(pDSP);
		pDSP->release();
		m_mapMasterDSP.erase(it);
	}
}

void CNtlSoundManager::SetGroupVolume(int iChannelGroup, float fVolume)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	m_apChannelGroup[iChannelGroup]->SetGroupVolume(fVolume);
}

float CNtlSoundManager::GetGroupVolume(int iChannelGroup)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return 1.f;

	return m_apChannelGroup[iChannelGroup]->GetGroupVolume();
}

void CNtlSoundManager::SetGroupEffect(int iChannelGroup, FMOD_DSP_TYPE eType)
{
	if (!CNtlSoundGlobal::m_pFMODSystem)
		return;

	if (m_mapGroupDSP[iChannelGroup].find(eType) != m_mapGroupDSP[iChannelGroup].end())
		return;

	FMOD::DSP* pDSP = nullptr;
	FMOD_RESULT result = CNtlSoundGlobal::m_pFMODSystem->createDSPByType(eType, &pDSP);
	if (result == FMOD_OK && pDSP)
	{
		m_apChannelGroup[iChannelGroup]->GetFMODChannelGroup()->addDSP(0, pDSP);
		pDSP->setActive(true);
		m_mapGroupDSP[iChannelGroup][eType] = pDSP;
	}
}

void CNtlSoundManager::ReleaseGroupEffect(int iChannelGroup, FMOD_DSP_TYPE eType)
{
	auto it = m_mapGroupDSP[iChannelGroup].find(eType);
	if (it != m_mapGroupDSP[iChannelGroup].end())
	{
		FMOD::DSP* pDSP = it->second;
		pDSP->setActive(false);
		m_apChannelGroup[iChannelGroup]->GetFMODChannelGroup()->removeDSP(pDSP);
		pDSP->release();
		m_mapGroupDSP[iChannelGroup].erase(it);
	}
}

void CNtlSoundManager::SetChannelVolume(SOUND_HANDLE hHandle, float fVolume)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	for( int i = CHANNEL_GROUP_FIRST ; i < NUM_CHANNEL_GROUP ; ++i )
	{
		if( m_apChannelGroup[i]->SetChannelVolume(hHandle, fVolume) )
			return;
	}	
}

float CNtlSoundManager::GetChannelVolume(SOUND_HANDLE hHandle)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return -1.f;

	CNtlSound* pSound = GetSound(hHandle);
	if( pSound )
		return pSound->m_tVolume.fMainVolume;

	return -1.f;
}

void CNtlSoundManager::SetChannelEffect(SOUND_HANDLE hHandle, FMOD_DSP_TYPE eType)
{
	if (!CNtlSoundGlobal::m_pFMODSystem)
		return;

	CNtlSound* pSound = GetSound(hHandle);
	if (pSound && pSound->m_pFMODChannel)
	{
		FMOD::DSP* pDSP = nullptr;
		FMOD_RESULT result = CNtlSoundGlobal::m_pFMODSystem->createDSPByType(eType, &pDSP);
		if (result == FMOD_OK && pDSP)
		{
			pSound->m_pFMODChannel->addDSP(0, pDSP);
			pDSP->setActive(true);
		}
	}
}

void CNtlSoundManager::ReleaseChannelEffect(SOUND_HANDLE hHandle, FMOD_DSP_TYPE eType)
{
	if (!CNtlSoundGlobal::m_pFMODSystem)
		return;

	CNtlSound* pSound = GetSound(hHandle);
	if (pSound && pSound->m_pFMODChannel)
	{
		int numDSPs = 0;
		pSound->m_pFMODChannel->getNumDSPs(&numDSPs);

		for (int i = 0; i < numDSPs; ++i)
		{
			FMOD::DSP* pDSP = nullptr;
			pSound->m_pFMODChannel->getDSP(i, &pDSP);
			if (pDSP)
			{
				FMOD_DSP_TYPE type;
				pDSP->getType(&type);
				if (type == eType)
				{
					pDSP->setActive(false);
					pSound->m_pFMODChannel->removeDSP(pDSP);
					pDSP->release();
					break;
				}
			}
		}
	}
}

int CNtlSoundManager::CanPlay(sNtlSoundPlayParameta* pParameta)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return SOUNDRESULT_OK;

	if( !IsExistGroup(pParameta->iChannelGroup) )
	{
		Logic_NtlSoundLog("CNtlSoundManager::CanPlay, Not exist channel group", pParameta->iChannelGroup, pParameta->pcFileName);
		return SOUNDRESULT_INVALID_CHANNELGROUP;
	}

	if( !pParameta->pcFileName || strlen(pParameta->pcFileName) == 0 )
	{
		Logic_NtlSoundLog("CNtlSoundManager::CanPlay, Empty sound file name", pParameta->iChannelGroup, pParameta->pcFileName);
		return SOUNDRESULT_EMPTY_FILENAME;
	}

	// Checks whether the sound range is playable in each channel group
	if( !IsValidGroupRange(pParameta->iChannelGroup, pParameta->fXPos, pParameta->fYPos, pParameta->fZPos) )
	{
#ifdef SOUND_DEBUG_LOG
		Logic_NtlSoundLog("CNtlSoundManager::CanPlay, out of range", pParameta->iChannelGroup, pParameta->pcFileName);
#endif
		return SOUNDRESULT_OUT_OF_RANGE;
	}

	int iResult;

	// °¢ channelGroup ´Ü¿¡¼­ÀÇ Play °¡´É ¿©ºÎ ÆÇ´Ü
	iResult = m_apChannelGroup[pParameta->iChannelGroup]->CanPlay(pParameta->pcFileName);
	if( iResult != SOUNDRESULT_OK )
		return iResult;

	// no more extra channels
	// Stop any channels in the most subchannel group and play a new sound
	// do. If the sound you want to play is the lowest group, it will not play.
	// Also, Jingle Music, BGM groups are not interrupted due to their priority.
	int iChannelCount = 0;
	CNtlSoundGlobal::m_pFMODSystem->getChannelsPlaying(&iChannelCount);

	if( iChannelCount  >= MAX_DBO_CHANNELS )
	{	// Á¦ÇÑµÈ ÃÖ´ë Ã¤³Î °¹¼ö¸¦ ³Ñ¾ú´Ù. ¿ì¼± ¼øÀ§°¡ ³·Àº Ã¤³ÎÀ» Á¦¿Ü½ÃÅ²´Ù.
		int iLowRankChannelGroup = NUM_CHANNEL_GROUP - 1;
		unsigned int uiCurChannelCount; // Ã¤³Î ±×·ì¿¡ ¼ÓÇÑ ÇÃ·¹ÀÌÁßÀÎ Ã¤³Î

		for( ; iLowRankChannelGroup > pParameta->iChannelGroup &&
			iLowRankChannelGroup > CHANNEL_GROUP_AMBIENT_MUSIC ; --iLowRankChannelGroup )
		{				
			uiCurChannelCount = m_apChannelGroup[iLowRankChannelGroup]->GetPlayingChannels();

			if( uiCurChannelCount > 0 )
			{	// ÇÏÀ§ ±×·ì¿¡ ÇÃ·¹ÀÌÁßÀÎ Ã¤³ÎÀÌ ÀÖ´Ù¸é °­Á¦ Release ½ÃÅ²´Ù.
				if( m_apChannelGroup[iLowRankChannelGroup]->ReleaseLowRankChannel() )
					return SOUNDRESULT_OK;
			}
		}
	}

	return SOUNDRESULT_OK;
}

int CNtlSoundManager::CanPlay(int iChannelGroups, const char* pcName, float fPosX, float fPosY, float fPosZ)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return SOUNDRESULT_OK;

	if( !IsExistGroup(iChannelGroups) )
	{
		Logic_NtlSoundLog("CNtlSoundManager::CanPlay, Not exist channel group", iChannelGroups, pcName);
		return SOUNDRESULT_INVALID_CHANNELGROUP;
	}

	if( !pcName || strlen(pcName) == 0 )
	{
		Logic_NtlSoundLog("CNtlSoundManager::CanPlay, Empty sound file name", iChannelGroups, pcName);
		return SOUNDRESULT_EMPTY_FILENAME;
	}

	// °¢ Ã¤³Î±×·ìº° ¿¬ÁÖ °¡´ÉÇÑ Áö¿ª ¹üÀ§ÀÇ »ç¿îµåÀÎÁö °Ë»ç
	if( !IsValidGroupRange(iChannelGroups, fPosX, fPosY, fPosZ) )
	{
#ifdef SOUND_DEBUG_LOG
		Logic_NtlSoundLog("CNtlSoundManager::CanPlay, out of range", iChannelGroups, pcName);
#endif
		return SOUNDRESULT_OUT_OF_RANGE;
	}

	int iResult;

	// °¢ channelGroup ´Ü¿¡¼­ÀÇ Play °¡´É ¿©ºÎ ÆÇ´Ü
	iResult = m_apChannelGroup[iChannelGroups]->CanPlay(pcName);
	if( iResult != SOUNDRESULT_OK )
		return iResult;

	// ´õ ÀÌ»ó ¿©ºÐÀÇ Ã¤³ÎÀÌ ¾øÀ» ½Ã
	// °¡Àå ÇÏÀ§ Ã¤³Î ±×·ìÀÇ ÀÓÀÇÀÇ Ã¤³ÎÀ» Áß´Ü½ÃÅ°°í »õ·Î¿î »ç¿îµå¸¦ ÇÃ·¹ÀÌ
	// ÇÑ´Ù. »õ·Î ÇÃ·¹ÀÌÇÏ·Á´Â »ç¿îµå°¡ ÃÖÇÏÀ§ ±×·ìÀÌ¶ó¸é ÇÃ·¹ÀÌµÇÁö ¾Ê´Â´Ù.
	// ¶ÇÇÑ Jingle Music, BGM ±×·ìÀº ¿ì¼±¼øÀ§¿¡ ¿µÇâÀ» ¹Þ¾Æ Áß´ÜµÇÁö ¾Ê´Â´Ù.
	int iChannelCount = 0;
	CNtlSoundGlobal::m_pFMODSystem->getChannelsPlaying(&iChannelCount);

	if( iChannelCount  >= MAX_DBO_CHANNELS )
	{	// Á¦ÇÑµÈ ÃÖ´ë Ã¤³Î °¹¼ö¸¦ ³Ñ¾ú´Ù. ¿ì¼± ¼øÀ§°¡ ³·Àº Ã¤³ÎÀ» Á¦¿Ü½ÃÅ²´Ù.
		int iLowRankChannelGroup = NUM_CHANNEL_GROUP - 1;
		unsigned int uiCurChannelCount; // Ã¤³Î ±×·ì¿¡ ¼ÓÇÑ ÇÃ·¹ÀÌÁßÀÎ Ã¤³Î

		for( ; iLowRankChannelGroup > iChannelGroups &&
			iLowRankChannelGroup > CHANNEL_GROUP_AMBIENT_MUSIC ; --iLowRankChannelGroup )
		{				
			uiCurChannelCount = m_apChannelGroup[iLowRankChannelGroup]->GetPlayingChannels();

			if( uiCurChannelCount > 0 )
			{	// ÇÏÀ§ ±×·ì¿¡ ÇÃ·¹ÀÌÁßÀÎ Ã¤³ÎÀÌ ÀÖ´Ù¸é °­Á¦ Release ½ÃÅ²´Ù.
				if( m_apChannelGroup[iLowRankChannelGroup]->ReleaseLowRankChannel() )
					return SOUNDRESULT_OK;
			}
		}
	}

	return SOUNDRESULT_OK;
}

bool CNtlSoundManager::IsValidGroupRange(int iChannelGroup, float fPosX, float fPosY, float fPosZ)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return false;

	switch(iChannelGroup)
	{
	case CHANNEL_GROUP_UI_SOUND:
	case CHANNEL_GROUP_JINGLE_MUSIC_WITHOUT_FADE:
	case CHANNEL_GROUP_JINGLE_MUSIC:
	case CHANNEL_GROUP_FLASH_MUSIC:
	case CHANNEL_GROUP_BGM:
	case CHANNEL_GROUP_AMBIENT_MUSIC:
	case CHANNEL_GROUP_OBJECT_MUSIC:
	case CHANNEL_GROUP_WEATHER_MUSIC:
		break;
	case CHANNEL_GROUP_AVATAR_VOICE_SOUND:
	case CHANNEL_GROUP_AVATAR_EFFECT_SOUND:
	case CHANNEL_GROUP_VOICE_SOUND:
	case CHANNEL_GROUP_EFFECT_SOUND:
	case CHANNEL_GROUP_WEATHER_EFFECT_SOUND:
		{
			// When requesting Play from PLSound, set the position to 0,0,0 so that you cannot actually hear the sound.
			// It is a structure that readjusts the position after Play request
			//if( LengthFromListenerToSound(fPosX, fPosY, fPosZ) > m_apChannelGroup[iChannelGroup]->GetValidGroupRange() )
			//	return false;

			break;
		}
	default:
		{
			NTL_ASSERT(false, "CNtlSoundManager::IsValidGroupRange, Invalid sound channel group type : " << iChannelGroup);
			break;
		}
	}

	return true;
}

float CNtlSoundManager::LengthFromListenerToSound(float fPosX, float fPosY, float fPosZ)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return 0.f;

	FMOD_VECTOR listenerPos;
	CNtlSoundGlobal::m_pFMODSystem->get3DListenerAttributes(0, &listenerPos, 0, 0, 0);

	listenerPos.x = dCONVERT_COORDINATE_X(listenerPos.x);

	float x = (listenerPos.x - fPosX);
	float y = (listenerPos.y - fPosY);
	float z = (listenerPos.z - fPosZ);

	float fLength = sqrt( x*x + y*y + z*z );

	return fLength;
}

CNtlSound* CNtlSoundManager::GetSound(SOUND_HANDLE hHandle)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return NULL;

	CNtlSound* pSound;
	for( int i = CHANNEL_GROUP_FIRST ; i < NUM_CHANNEL_GROUP ; ++i )
	{
		pSound = m_apChannelGroup[i]->GetSound(hHandle);
		if( pSound )
			return pSound;
	}

	return NULL;
}

void CNtlSoundManager::FadeIn(int iGroup, float fDestVolume, unsigned int ulTime)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	GetFadeInOut()->FadeIn(m_apChannelGroup[iGroup], fDestVolume, ulTime);
}

void CNtlSoundManager::FadeOut(int iGroup, float fDestVolume, unsigned int ulTime)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	GetFadeInOut()->FadeOut(m_apChannelGroup[iGroup], fDestVolume, ulTime);
}

void CNtlSoundManager::FadeIn(SOUND_HANDLE hHandle, float fDestVolume, unsigned int ulTime)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	CNtlSound* pSound = GetSound(hHandle);
	if( pSound )
	{
		GetFadeInOut()->FadeIn(pSound, fDestVolume, ulTime);
	}
}

void CNtlSoundManager::FadeOut(SOUND_HANDLE hHandle, float fDestVolume, unsigned int ulTime)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	CNtlSound* pSound = GetSound(hHandle);
	if( pSound )
	{
		GetFadeInOut()->FadeOut(pSound, fDestVolume, ulTime);
	}
}
unsigned int CNtlSoundManager::GetPlayingChannels() 
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return 0;

	int iChannels;
	CNtlSoundGlobal::m_pFMODSystem->getChannelsPlaying(&iChannels);
	return iChannels;
}

void CNtlSoundManager::SetMute(bool bMute)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	if(m_pMasterChannelGroup)
		m_pMasterChannelGroup->setMute(bMute);
}

void CNtlSoundManager::SetMute(int iChannelGroup, bool bMute)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	m_apChannelGroup[iChannelGroup]->SetMute(bMute);
}

bool CNtlSoundManager::IsMute(int iChannelGroup)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return false;

	return m_apChannelGroup[iChannelGroup]->IsMute();
}

void CNtlSoundManager::SetProhibition(int iChannelGroup, RwBool bProhibition)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return;

	m_apChannelGroup[iChannelGroup]->SetProhibition(bProhibition);
}

RwBool CNtlSoundManager::IsProhibition(int iChannelGroup)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return FALSE;

	return m_apChannelGroup[iChannelGroup]->IsProhibition();
}

bool CNtlSoundManager::IsExistGroup(int iChannelGroup)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return false;

	if( CHANNEL_GROUP_FIRST <= iChannelGroup && iChannelGroup < NUM_CHANNEL_GROUP )
		return true;

	return false;
}

CNtlChannelGroup* CNtlSoundManager::GetChannelGroup(int iChannelGroup)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return NULL;

	return m_apChannelGroup[iChannelGroup];
}

char* CNtlSoundManager::GetGroupName(int iChannelGroup)
{
	if( !CNtlSoundGlobal::m_pFMODSystem )
		return NULL;

	static char acBuffer[256] = "";
	FMOD::ChannelGroup* pChannelGroup = m_apChannelGroup[iChannelGroup]->GetFMODChannelGroup();
	pChannelGroup->getName(acBuffer, 256);

	return acBuffer;
}
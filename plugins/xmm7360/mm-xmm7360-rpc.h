/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2020 Marinus Enzinger
 */

#ifndef MM_XMM7360_RPC_H
#define MM_XMM7360_RPC_H

#include <gmodule.h>
#include <glib.h>

typedef enum {
    UtaMsSimOpenReq = 1,
    UtaMsSimApduCmdReq = 2,
    UtaMsSimApplicationReq = 4,
    UtaMsSimDecodeFcp = 6,
    UtaMsSimPbReadEntryReq = 0xD,
    UtaMsSimGenPinReq = 0xF,
    UtaMsSimModifyLockReq = 0x11,
    UtaMsSimTkProactiveCommandRsp = 0x16,
    UtaMsSimTkEnvelopeCommandReq = 0x17,
    UtaMsSimTkTerminalProfileReadReq = 0x19,
    UtaMsSimTkRegisterHandler = 0x1C,
    UtaMsSimTkDeregisterHandler = 0x1D,
    UtaMsCpsSetModeReq = 0x1F,
    UtaMsCpsSetStackModeConfiguration = 0x20,
    UtaMsCpsSetSimModeConfiguration = 0x21,
    UtaMsCpsReadImei = 0x23,
    UtaMsCallCsInit = 0x24,
    UtaMsCbsInit = 0x25,
    UtaMsSsInit = 0x26,
    UtaMsSsSendUssdReq = 0x27,
    UtaMsSsRespondUssd = 0x28,
    UtaMsSsAbort = 0x29,
    UtaMsSmsInit = 0x30,
    UtaMsSmsSendReq = 0x31,
    UtaMsSmsSetMemoryAvailableReq = 0x34,
    UtaMsSmsIncomingSmsAck = 0x36,
    UtaMsSmsSimMsgCountReq = 0x38,
    UtaMsCallPsInitialize = 0x3A,
    UtaMsCallPsObtainPdpContextId = 0x3B,
    UtaMsCallPsReleasePdpContextId = 0x3C,
    UtaMsCallPsDefinePrimaryReq = 0x3D,
    UtaMsCallPsUndefinePrimaryReq = 0x3F,
    UtaMsCallPsGetPrimaryReq = 0x41,
    UtaMsCallPsSetAuthenticationReq = 0x43,
    UtaMsCallPsSetDnsReq = 0x45,
    UtaMsCallPsGetNegotiatedDnsReq = 0x47,
    UtaMsCallPsGetNegIpAddrReq = 0x49,
    UtaMsCallPsActivateReq = 0x4B,
    UtaMsCallPsDeactivateReq = 0x4E,
    UtaMsCallPsConnectReq = 0x51,
    UtaMsNetOpen = 0x53,
    UtaMsNetSetRadioSignalReporting = 0x54,
    UtaMsNetSingleShotRadioSignalReportingReq = 0x55,
    UtaMsNetAttachReq = 0x5C,
    UtaMsNetPsAttachReq = 0x5D,
    UtaMsNetPsDetachReq = 0x5E,
    UtaMsNetScanReq = 0x5F,
    UtaMsNetScanAbort = 0x60,
    UtaMsNetPowerDownReq = 0x61,
    UtaMsNetExtScanReq = 0x62,
    UtaMsNetSetFdConfigReq = 0x6E,
    UtaMsNetGetFdConfigReq = 0x71,
    UtaMsNetConfigureNetworkModeReq = 0x73,
    UtaMsNetRatModeStatusReq = 0x76,
    UtaNvmRead = 0x79,
    UtaNvmWrite = 0x7A,
    UtaNvmWriteCommit = 0x7B,
    UtaSysGetInfo = 0x7C,
    UtaRPCPSConnectSetupReq = 0x7D,
    UtaRPCPsConnectToDatachannelReq = 0x7E,
    UtaRPCPSConnectReleaseReq = 0x7F,
    UtaMsNetDcSetVoiceDomainPreferenceConfigReq = 0x80,
    UtaMsCallCsSetupVoiceCallReq = 0x82,
    UtaMsCallCsReleaseCallReq = 0x88,
    UtaMsCallCsAcceptCallReq = 0x8D,
    UtaMsCallCsSwapCallsReq = 0x90,
    UtaMsCallCsHoldCallReq = 0x92,
    UtaMsCallCsRetrieveCallReq = 0x94,
    UtaMsCallCsSplitMptyReq = 0x96,
    UtaMsCallCsJoinCallsReq = 0x98,
    UtaMsCallCsTransferCallsReq = 0x9A,
    UtaMsCallCsStartDtmfReq = 0x9C,
    UtaMsCallCsStopDtmfReq = 0x9E,
    UtaMsCallCsSetUus1Info = 0xA6,
    UtaMsCallCsSetTtyDeviceMode = 0xA7,
    UtaMsCallCsGetTtyDeviceMode = 0xA8,
    UtaMsCallMultimediaSetupCallReq = 0xAC,
    UtaMsCallMultimediaUpdateCallReq = 0xAD,
    UtaMsCpsSetSimModeReq = 0xB0,
    UtaMsSsCallForwardReq = 0xB2,
    UtaMsSsCallWaitingReq = 0xB4,
    UtaMsSsCallBarringReq = 0xB6,
    UtaMsSsIdentificationReq = 0xB8,
    UtaMsSmsSetSendMoreMessagesStatus = 0xBA,
    UtaMsSmsDataDownloadReq = 0xBB,
    UtaMsSmsDataDownloadAck = 0xBD,
    UtaMsCallPsGetNegQosReq = 0xBE,
    UtaMsCallPsGetTftReq = 0xC0,
    UtaMsCallPsSetPcoReq = 0xC2,
    UtaMsCallPsGetNwPcoReq = 0xC4,
    UtaMsCallPsNwActivateAcceptReq = 0xC7,
    UtaMsCallPsNwActivateRejectReq = 0xC9,
    UtaMsCallPsSetDataPrefReq = 0xCD,
    UtaMsCbsStartReq = 0xCF,
    UtaMsCbsStopReq = 0xD0,
    UtaMsCbsSetMsgFilter = 0xD3,
    UtaMsCbsGetMsgFilter = 0xD4,
    UtaMsCbsEtwsConfigReq = 0xD6,
    UtaMsCbsEtwsStartReq = 0xD8,
    UtaMsCbsEtwsStopReq = 0xDA,
    UtaMsCpsNvmWrite = 0xDE,
    UtaMsCpsNvmRead = 0xDF,
    UtaMsNetConfigureRxDiversityDarp = 0xE0,
    UtaMsNetLdrGetApnParameterList = 0xE2,
    UtaMsNetTimeInfoReadReq = 0xE3,
    UtaMsNetSetCsgConfigReq = 0xE6,
    UtaMsNetBandStatusReq = 0xE7,
    UtaMsNetGetExtendedRadioSignalInfoReq = 0xEC,
    UtaMsNetDetachReq = 0xEF,
    UtaMsNetSelectGprsClassReq = 0xF1,
    UtaMsNetGetCsgConfigReq = 0xF3,
    UtaMsNetCsServiceNotificationAccept = 0xF4,
    UtaMsNetSingleShotFdReq = 0xF9,
    UtaMsSimPbLocationReq = 0xFB,
    UtaMsSimPbReadGasEntryReq = 0xFD,
    UtaMsSimPbWriteEntryReq = 0xFF,
    UtaMsSimPbGetMetaInformationReq = 0x101,
    UtaMsSimPbUsimPbSelectReq = 0x103,
    UtaMsSimPbGetFreeRecordsReq = 0x105,
    UtaMsSimCreateReadBinaryApdu = 0x10A,
    UtaMsSimCreateUpdateBinaryApdu = 0x10B,
    UtaMsSimAnalyseReadResult = 0x10C,
    UtaMsSimSetFdnReq = 0x10E,
    SetApScreenState = 0x110,
    UtaIoCtl = 0x111,
    UtaIdcApMsgSetReq = 0x114,
    UtaIdcApMsgGetReq = 0x115,
    UtaIdcEnbleReq = 0x116,
    UtaIdcCwsMsgSetReq = 0x119,
    UtaIdcCwsMsgGetReq = 0x11A,
    UtaIdcSubscribeIndications = 0x11C,
    UtaIdcUnsubscribeIndications = 0x11D,
    UtaBootPrepareShutdownReq = 0x11F,
    UtaBootShutdownReq = 0x120,
    UtaRfMaxTxPwrSet2g = 0x121,
    UtaRfMaxTxPwrSet3g = 0x122,
    UtaRfMaxTxPwrSet4g = 0x123,
    UtaFreqInfoActivateReq = 0x128,
    UtaFreqInfoGetFreqInfoReq = 0x129,
    UtaFreqInfoDeactivateReq = 0x12A,
    UtaFreqInfoRegisterIndications = 0x12B,
    UtaFreqInfoDeregisterIndications = 0x12C,
    UtaModeSetReq = 0x12F,
    UtaNvmFlushSync = 0x130,
    UtaProdRegisterGtiCallbackFunc = 0x132,
    UtaProdGtiCmdReq = 0x133,
    UtaCellTimeStampReq = 0x134,
    UtaMsSsLcsInit = 0x136,
    UtaMsSsLcsMoLocationReq = 0x137,
    UtaMsSsLcsMtlrNotificationRsp = 0x139,
    UtaMsCpAssistanceDataInjectReq = 0x13C,
    UtaMsCpResetAssistanceData = 0x13D,
    UtaMsCpPosMeasurementReq = 0x140,
    UtaMsCpPosMeasurementAbortReq = 0x142,
    UtaMsCpPosEnableMeasurementReport = 0x144,
    UtaMsCpPosDisableMeasurementReport = 0x145,
    UtaMsSimTkInit = 0x146,
    UtaMsSimTkExecSmsPpRsp = 0x148,
    UtaMsSimTkExecSimInitiatedCallRsp = 0x14A,
    UtaMsSimTkExecSsUssdRsp = 0x14C,
    UtaMsSimTkExecDtmfRsp = 0x14E,
    UtaMsSimTkStopDtmfReq = 0x150,
    UtaMsSimTkRefreshConfirmRsp = 0x152,
    UtaMsSimTkRefreshFcnRsp = 0x154,
    UtaMsSimTkControlReq = 0x155,
    UtaMsSimTkTerminalProfileDownloadReq = 0x157,
    UtaMs3gpp2SmsSendReq = 0x15A,
    UtaMs3gpp2SmsSubscribeIndications = 0x15C,
    UtaMs3gpp2SmsUnsubscribeIndications = 0x15D,
    RpcGetRemoteVerInfo = 0x15E,
    UtaMsMetricsRegisterHandler = 0x160,
    UtaMsMetricsDeregisterHandler = 0x161,
    UtaMsMetricsSetOptions = 0x162,
    UtaMsMetricsTrigger = 0x163,
    UtaMsEmbmsInit = 0x164,
    UtaMsEmbmsSetServiceReq = 0x165,
    UtaMsEmbmsMbsfnAreaConfigReq = 0x166,
    UtaMsEmbmsSessionConfigReq = 0x167,
    UtaMsEmbmsSetInterestedTMGIListReq = 0x168,
    UtaMsEmbmsSetInterestedSAIFreqReq = 0x169,
    UtaImsSubscribeIndications = 0x176,
    UtaImsUnsubscribeIndications = 0x177,
    UtaImsGetFrameworkState = 0x178,
    UtaRtcGetDatetime = 0x179,
    UtaMsSimAnalyseSimApduResult = 0x17A,
    UtaMsSimOpenChannelReq = 0x17B,
    UtaMsSimCloseChannelReq = 0x17D,
    UtaMsSimSetBdnReq = 0x17F,
    UtaMsSetSimStackMappingReq = 0x181,
    UtaMsGetSimStackMappingReq = 0x183,
    UtaMsNetSetRadioSignalReportingConfiguration = 0x188,
    UtaPCIeEnumerationextTout = 0x189,
    UtaMsSimTkSetTerminalCapabilityReq = 0x18A,
    UtaMsSimTkReadTerminalCapabilityReq = 0x18C,
    CsiFccLockQueryReq = 0x18E,
    CsiFccLockGenChallengeReq = 0x190,
    CsiFccLockVerChallengeReq = 0x192,
    UtaSensorOpenReq = 0x194,
    UtaSensorCloseExt = 0x197,
    UtaSensorStartExt = 0x198,
    UtaSensorSetAlarmParamExt = 0x199,
    UtaSensorSetSchedulerParamExt = 0x19A,
    CsiSioIpFilterCntrlSetReq = 0x19B,
    UtaMsAccCurrentFreqInfoReq = 0x19D,
    CsiTrcAtCmndReq = 0x1A0,
    UtaMsSimApduCmdExtReq = 0x1A2,
    UtaMsNetGetPlmnNameInfoReq = 0x1A4,
    UtaMsNetGetCountryListReq = 0x1A7,
    UtaMsNetExtConfigureNetworkModeReq = 0x1A9,
    UtaMsNetExtBandStatusReq = 0x1AC,
    UtaMsCallPsAttachApnConfigReq = 0x1AF,
    CsiMsCallPsInitialize = 0x1B1,
    UtaAudioEnableSource = 0x1B2,
    UtaAudioDisableSource = 0x1B3,
    UtaAudioConfigureDestinationExt = 0x1B4,
    UtaAudioSetDestinationsForSource = 0x1B5,
    UtaAudioSetVolumeForSource = 0x1B6,
    UtaAudioSetMuteForSourceExt = 0x1B7,
    UtaAudioSetVolumeForDestination = 0x1B8,
    UtaAudioSetMuteForDestinationExt = 0x1B9,
    UtaAudioConfigureSourceExt = 0x1BA,
    UtaAudioSetDestinationsForSourceExt = 0x1BB,
    UtaRPCScreenControlReq = 0x1BC,
    UtaMsCallPsReadContextStatusReq = 0x1BD,
    CsiMsSimAccessGetSimStateInfoReq = 0x1BF,
    CsiMsNetGetRegistrationInfoReq = 0x1C1,
    CsiSioIpFilterNewCntrlSetReq = 0x1C3,
    CsiMsNetLdrGetApnPlmnParameterListReq = 0x1C5,
    RPCGetAPIParamChangedBitmap = 0x1c8,
} Xmm7360RpcCallIds;

typedef enum {
    UtaMsSimApduCmdRspCb = 0x003,
    UtaMsSimApplicationRspCb = 0x005,
    UtaMsSimInfoIndCb = 0x007,
    UtaMsSimInitIndCb = 0x008,
    UtaMsSimFullAccessIndCb = 0x009,
    UtaMsSimErrorIndCb = 0x00a,
    UtaMsSimCardIndCb = 0x00b,
    UtaMsSimApplicationIndCb = 0x00c,
    UtaMsSimPbReadEntryRspCb = 0x00e,
    UtaMsSimGenPinRspCb = 0x010,
    UtaMsSimModifyLockRspCb = 0x012,
    UtaMsSimLockStatusIndCb = 0x013,
    UtaMsSimTkMoSmsControlInfoIndCb = 0x014,
    UtaMsSimTkProactiveCommandIndCb = 0x015,
    UtaMsSimTkEnvelopeResIndCb = 0x018,
    UtaMsSimTkTerminalProfileReadRspCb = 0x01a,
    UtaSimTkProactiveCommandHandlerFunc = 0x01b,
    UtaMsCpsSetModeRsp = 0x01e,
    UtaMsCpsSetModeIndCb = 0x022,
    UtaMsSsNetworkErrorIndCb = 0x02a,
    UtaMsSsNetworkRejectIndCb = 0x02b,
    UtaMsSsNetworkGsmCauseIndCb = 0x02c,
    UtaMsSsUssdRspCb = 0x02d,
    UtaMsSsUssdIndCb = 0x02e,
    UtaMsSsEndIndCb = 0x02f,
    UtaMsSmsIncomingIndCb = 0x032,
    UtaMsSmsSendRspCb = 0x033,
    UtaMsSmsSetMemoryAvailableRspCb = 0x035,
    UtaMsSmsSimMsgCacheFinishedIndCb = 0x037,
    UtaMsSmsSimMsgCountRspCb = 0x039,
    UtaMsCallPsDefinePrimaryRspCb = 0x03e,
    UtaMsCallPsUndefinePrimaryRspCb = 0x040,
    UtaMsCallPsGetPrimaryRspCb = 0x042,
    UtaMsCallPsSetAuthenticationRspCb = 0x044,
    UtaMsCallPsSetDnsRspCb = 0x046,
    UtaMsCallPsGetNegotiatedDnsRspCb = 0x048,
    UtaMsCallPsGetNegIpAddrRspCb = 0x04a,
    UtaMsCallPsActivateRspCb = 0x04c,
    UtaMsCallPsActivateStatusIndCb = 0x04d,
    UtaMsCallPsDeactivateRspCb = 0x04f,
    UtaMsCallPsDeactivateIndCb = 0x050,
    UtaMsCallPsConnectRspCb = 0x052,
    UtaMsNetSingleShotRadioSignalReportingRspCb = 0x056,
    UtaMsNetCellInfoIndCb = 0x057,
    UtaMsNetConnectionInfoIndCb = 0x058,
    UtaMsNetHspaInfoIndCb = 0x059,
    UtaMsNetRadioSignalIndCb = 0x05a,
    UtaMsNetCellChangeIndCb = 0x05b,
    UtaMsNetAttachRspCb = 0x063,
    UtaMsNetPsAttachRspCb = 0x064,
    UtaMsNetPsDetachRspCb = 0x065,
    UtaMsNetScanRspCb = 0x066,
    UtaMsNetPowerDownRspCb = 0x067,
    UtaMsNetExtScanRspCb = 0x068,
    UtaMsNetPsAttachIndCb = 0x069,
    UtaMsNetPsDetachIndCb = 0x06a,
    UtaMsNetRegistrationInfoIndCb = 0x06b,
    UtaMsNetIsAttachAllowedIndCb = 0x06c,
    UtaMsNetGprsClassIndCb = 0x06d,
    UtaMsNetSetFdConfigRspCb = 0x06f,
    UtaMsNetFdConfigIndCb = 0x070,
    UtaMsNetGetFdConfigRspCb = 0x072,
    UtaMsNetConfigureNetworkModeRspCb = 0x074,
    UtaMsNetNetworkModeChangeIndCb = 0x075,
    UtaMsNetRatModeStatusRspCb = 0x077,
    UtaMsNetRatModeStatusIndCb = 0x078,
    UtaMsNetDcSetVoiceDomainPreferenceConfigRspCb = 0x081,
    UtaMsCallCsSetupCallRspCb = 0x083,
    UtaMsCallCsDialingIndCb = 0x084,
    UtaMsCallCsAlertingIndCb = 0x085,
    UtaMsCallCsCtmInfoIndCb = 0x086,
    UtaMsCallCsConnectedIndCb = 0x087,
    UtaMsCallCsReleaseCallRspCb = 0x089,
    UtaMsCallCsDisconnectingIndCb = 0x08a,
    UtaMsCallCsDisconnectedIndCb = 0x08b,
    UtaMsCallCsIncomingCallIndCb = 0x08c,
    UtaMsCallCsAcceptCallRspCb = 0x08e,
    UtaMsCallCsProgressIndCb = 0x08f,
    UtaMsCallCsSwapCallsRspCb = 0x091,
    UtaMsCallCsHoldCallRspCb = 0x093,
    UtaMsCallCsRetrieveCallRspCb = 0x095,
    UtaMsCallCsSplitMptyRspCb = 0x097,
    UtaMsCallCsJoinCallsRspCb = 0x099,
    UtaMsCallCsTransferCallsRspCb = 0x09b,
    UtaMsCallCsStartDtmfRspCb = 0x09d,
    UtaMsCallCsStopDtmfRspCb = 0x09f,
    UtaMsCallCsStopDtmfExtRspCb = 0x0a0,
    UtaMsCallCsNotificationIndCb = 0x0a1,
    UtaMsCallCsCugInfoIndCb = 0x0a2,
    UtaMsCallCsCallingNameInfoIndCb = 0x0a3,
    UtaMsCallCsEmergencyNumberListIndCb = 0x0a4,
    UtaMsCallCsCallStatusIndCb = 0x0a5,
    UtaCallMultimediaGetMediaProfilesInfoRspCb = 0x0a9,
    UtaMsCallMultimediaSetupCallRspCb = 0x0aa,
    UtaMsCallMultimediaUpdateCallRspCb = 0x0ab,
    UtaMsCallCsVoimsSrvccHoStatusIndCb = 0x0ae,
    UtaMsCpsSetSimModeRsp = 0x0af,
    UtaMsCpsStartupIndCb = 0x0b1,
    UtaMsSsCallForwardRspCb = 0x0b3,
    UtaMsSsCallWaitingRspCb = 0x0b5,
    UtaMsSsCallBarringRspCb = 0x0b7,
    UtaMsSsIdentificationRspCb = 0x0b9,
    UtaMsSmsDataDownloadRspCb = 0x0bc,
    UtaMsCallPsGetNegQosRspCb = 0x0bf,
    UtaMsCallPsGetTftRspCb = 0x0c1,
    UtaMsCallPsSetPcoRspCb = 0x0c3,
    UtaMsCallPsGetNwPcoRspCb = 0x0c5,
    UtaMsCallPsNwActivateIndCb = 0x0c6,
    UtaMsCallPsNwActivateAcceptRspCb = 0x0c8,
    UtaMsCallPsModifyIndCb = 0x0ca,
    UtaMsCallPsSuspendIndCb = 0x0cb,
    UtaMsCallPsResumeIndCb = 0x0cc,
    UtaMsCallPsSetDataPrefRspCb = 0x0ce,
    UtaMsCbsStartRspCb = 0x0d1,
    UtaMsCbsStopRspCb = 0x0d2,
    UtaMsCbsNewMessageIndCb = 0x0d5,
    UtaMsCbsEtwsConfigRspCb = 0x0d7,
    UtaMsCbsEtwsStartRspCb = 0x0d9,
    UtaMsCbsEtwsStopRspCb = 0x0db,
    UtaMsCbsEtwsNotifyPrimaryWarningInd = 0x0dc,
    UtaMsCbsEtwsNotifySecondaryWarningInd = 0x0dd,
    UtaMsNetConfigureRxDiversityDarpIndCb = 0x0e1,
    UtaMsNetTimeInfoReadRspCb = 0x0e4,
    UtaMsNetTimeInfoIndCb = 0x0e5,
    UtaMsNetBandStatusRspCb = 0x0e8,
    UtaMsNetBandStatusIndCb = 0x0e9,
    UtaMsNetSetCsgConfigRspCb = 0x0ea,
    UtaMsNetGetCsgConfigRspCb = 0x0eb,
    UtaMsNetGetExtendedRadioSignalInfoRspCb = 0x0ed,
    UtaMsNetNitzInfoIndCb = 0x0ee,
    UtaMsNetDetachRspCb = 0x0f0,
    UtaMsNetSelectGprsClassRspCb = 0x0f2,
    UtaMsNetNetworkFeatureSupportInfoIndCb = 0x0f5,
    UtaMsNetEpsNetworkFeatureSupportInfoIndCb = 0x0f6,
    UtaMsNetCsServiceNotificationIndCb = 0x0f7,
    UtaMsNetDualSimServiceIndCb = 0x0f8,
    UtaMsNetSingleShotFdRspCb = 0x0fa,
    UtaMsSimPbGetLocationRspCb = 0x0fc,
    UtaMsSimPbReadGasEntryRspCb = 0x0fe,
    UtaMsSimPbWriteEntryRspCb = 0x100,
    UtaMsSimPbGetMetaInformationRspCb = 0x102,
    UtaMsSimPbUsimPbSelectRspCb = 0x104,
    UtaMsSimPbGetFreeRecordsRspCb = 0x106,
    UtaMsSimPbUsimPbReadyIndCb = 0x107,
    UtaMsSimPbCacheLoadFinishedIndCb = 0x108,
    UtaMsSimPbCacheLoadIndCb = 0x109,
    UtaMsSimGenPinIndCb = 0x10d,
    UtaMsSimFdnStateIndCb = 0x10f,
    UtaIdcApMsgSetRspCb = 0x112,
    UtaIdcApMsgGetRspCb = 0x113,
    UtaIdcCwsMsgSetRspCb = 0x117,
    UtaIdcCwsMsgGetRspCb = 0x118,
    UtaIdcCwsMsgIndCb = 0x11b,
    UtaBootPrepareShutdownRspCb = 0x11e,
    UtaFreqInfoActivateRspCb = 0x124,
    UtaFreqInfoDeactivateRspCb = 0x125,
    UtaFreqInfoGetFreqInfoRspCb = 0x126,
    UtaFreqInfoIndicationCb = 0x127,
    UtaModeSetRspCb = 0x12d,
    UtaModeStartupIndCb = 0x12e,
    UtaProdGtiCmdRspCb = 0x131,
    UtaCellTimeStampRspCb = 0x135,
    UtaMsSsLcsMoLocationRspCb = 0x138,
    UtaMsSsLcsCapabilitiesIndCb = 0x13a,
    UtaMsCpAssistanceDataInjectRspCb = 0x13b,
    UtaMsCpAssistanceDataNeededIndCb = 0x13e,
    UtaMsCpPosMeasurementRspCb = 0x13f,
    UtaMsCpPosMeasurementAbortRspCb = 0x141,
    UtaMsCpPosReportMeasurementIndCb = 0x143,
    UtaMsSimTkExecSmsPpIndCb = 0x147,
    UtaMsSimTkExecSimInitiatedCallIndCb = 0x149,
    UtaMsSimTkExecSsUssdIndCb = 0x14b,
    UtaMsSimTkExecDtmfIndCb = 0x14d,
    UtaMsSimTkExecDtmfEndIndCb = 0x14f,
    UtaMsSimTkRefreshConfirmIndCb = 0x151,
    UtaMsSimTkRefreshFcnIndCb = 0x153,
    UtaMsSimTkControlRspCb = 0x156,
    UtaMsSimTkTerminalProfileDownloadRspCb = 0x158,
    UtaMs3gpp2SmsSendRspCb = 0x159,
    UtaMs3gpp2SmsIncomingIndCb = 0x15b,
    UtaMetricsHandlerFunction = 0x15f,
    UtaMsEmbmsSetServiceRspCb = 0x16a,
    UtaMsEmbmsMbsfnAreaConfigRspCb = 0x16b,
    UtaMsEmbmsSessionConfigRspCb = 0x16c,
    UtaMsEmbmsSetInterestedTMGIListRspCb = 0x16d,
    UtaMsEmbmsSetInterestedSAIFreqRspCb = 0x16e,
    UtaMsEmbmsServiceIndCb = 0x16f,
    UtaMsEmbmsMBSFNAreaIndCb = 0x170,
    UtaMsEmbmsServicesListIndCb = 0x171,
    UtaMsEmbmsSAIListIndCb = 0x172,
    UtaMsEmbmsMpsInfoIndCb = 0x173,
    UtaImsStateChangedIndCb = 0x174,
    UtaImsServiceStateChangedIndCb = 0x175,
    UtaMsSimOpenChannelRspCb = 0x17c,
    UtaMsSimCloseChannelRspCb = 0x17e,
    UtaMsSimBdnStateIndCb = 0x180,
    UtaMsSetSimStackMappingRspCb = 0x182,
    UtaMsGetSimStackMappingRspCb = 0x184,
    UtaMsSimMccMncIndCb = 0x185,
    UtaMsSimTkTerminalResponseIndCb = 0x186,
    UtaMsNetRegisteredPlmnNameIndCb = 0x187,
    UtaMsSimTkSetTerminalCapabilityRspCb = 0x18b,
    UtaMsSimTkReadTerminalCapabilityRspCb = 0x18d,
    CsiFccLockQueryRspCb = 0x18f,
    CsiFccLockGenChallengeRspCb = 0x191,
    CsiFccLockVerChallengeRspCb = 0x193,
    UtaSensorOpenRspCb = 0x195,
    UtaSensorMeasIndCb = 0x196,
    CsiSioIpFilterCntrlSetRspCb = 0x19c,
    CsiSioIpFilterNewCntrlSetRspCb = 0x1c4,
    UtaMsAccCurrentFreqInfoRspCb = 0x19e,
    UtaMsAccCurrentFreqInfoIndCb = 0x19f,
    CsiTrcAtCmndRspCb = 0x1a1,
    UtaMsSimApduCmdExtRspCb = 0x1a3,
    UtaMsNetGetPlmnNameInfoRspCb = 0x1a5,
    UtaMsNetSib8TimeInfoIndCb = 0x1a6,
    UtaMsNetGetCountryListRspCb = 0x1a8,
    UtaMsNetExtConfigureNetworkModeRspCb = 0x1aa,
    UtaMsNetExtNetworkModeChangeIndCb = 0x1ab,
    UtaMsNetExtBandStatusRspCb = 0x1ad,
    UtaMsNetExtBandStatusIndCb = 0x1ae,
    UtaMsCallPsAttachApnConfigRspCb = 0x1b0,
    UtaMsCallPsReadContextStatusRspCb = 0x1be,
    CsiMsSimAccessGetSimStateInfoRspCb = 0x1c0,
    CsiMsNetGetRegistrationInfoRspCb = 0x1c2,
    CsiMsNetLdrGetApnPlmnParameterListRspCb = 0x1c6,
    UtaMsNetLdrApnParametersChangeIndCb = 0x1c7,
} Xmm7360RpcUnsolIds;

#define GET_RPC_INT(arg) (arg->type == BYTE ? arg->value.b : arg->type == SHORT ? arg->value.s : arg->type == LONG ? arg->value.l : -1)
typedef struct {
    enum {
        BYTE,
        SHORT,
        LONG,
        STRING
    } type;
    guint size;
    union {
        gint8 b;
        gint16 s;
        gint32 l;
        const gchar* string; 
    } value;
} rpc_arg;

typedef struct {
    guint32 tx_id;
    enum {
        RESPONSE,
        ASYNC_ACK,
        UNSOLICITED
    } type;
    guint32 code;
    GBytes *body;
    //contains rpc_args
    GArray* content;
} rpc_message;

rpc_message* xmm7360_rpc_alloc_message(void);

void xmm7360_rpc_free_message(rpc_message* msg);

typedef struct {
    gboolean attach_allowed;
    int fd;
} xmm7360_rpc;

/**
 * RPC Communication functions
 */
int xmm7360_rpc_init(xmm7360_rpc* rpc);

int xmm7360_rpc_pump(xmm7360_rpc* rpc, gboolean is_async, gboolean have_ack, guint32 tid_word);

int xmm7360_rpc_execute(xmm7360_rpc* rpc, gboolean is_async, GByteArray* body, rpc_message* res_ptr);

int xmm7360_rpc_handle_message(xmm7360_rpc* rpc, GBytes* message, rpc_message* res_ptr);

int xmm7360_rpc_handle_unsolicited(xmm7360_rpc* rpc, rpc_message* message);

/**
 * Pack a string into a byte array
 */
void pack_string(GByteArray* target, guint8* data, gsize val_len, guint length);

/**
 * Encode a integer
 */
void asn_int4(GByteArray* target, gint32 val);

/**
 * Retrieve an integer
 */
gint get_asn_int(GBytes* bytes, gsize* current_offset);

/**
 * Retrieve an encoded string
 */
GBytes* get_string(GBytes* bytes, gsize* current_offset);

/**
 * ##########################
 * Argument packing functions
 * ##########################
 */

GByteArray* pack(guint count, rpc_arg* args);

GByteArray* pack_uta_ms_call_ps_connect_req(void);

GByteArray* pack_uta_ms_call_ps_get_negotiated_dns_req(void);

GByteArray* pack_uta_ms_call_ps_get_get_ip_addr_req(void);

GByteArray* pack_uta_ms_net_attach_req(void);

GByteArray* pack_uta_rpc_ps_connect_to_datachannel_req(void);

GByteArray* pack_uta_sys_get_info(gint index);

GByteArray* pack_uta_ms_call_ps_attach_apn_config_req(gchar* apn);

/**
 * Argument unpacking functions
 */

//unpacked strings live as long as data is not freed
gboolean unpack(GBytes* data, guint count, rpc_arg* args);

gboolean unpack_uta_ms_call_ps_get_neg_ip_addr_req(GBytes* data, guint32* ip1, guint32* ip2, guint32* ip3);

gboolean unpack_uta_ms_call_ps_get_neg_dns_req(GBytes* data, guint32* ipv4_1, guint32* ipv4_2);

#endif /* MM_XMM7360_RPC_H */
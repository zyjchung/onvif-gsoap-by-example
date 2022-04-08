/**
 * ./ipconvif -cIp '192.168.1.53' -cUsr 'admin' -cPwd 'admin' -fIp '192.168.1.51:21' -fUsr 'ftpuser' -fPwd 'Ftpftp123
 *
 */

#include <iostream>
#include "stdio.h"
#include "wsdd.nsmap"
#include "plugin/wsseapi.h"
#include "plugin/wsaapi.h"
#include <openssl/rsa.h>
#include "ErrorLog.h"

#include "include/soapDeviceBindingProxy.h"
#include "include/soapMediaBindingProxy.h"
#include "include/soapPTZBindingProxy.h"

#include "include/soapPullPointSubscriptionBindingProxy.h"
#include "include/soapRemoteDiscoveryBindingProxy.h"

#include <stdarg.h>  // For va_start, etc.
#include <memory> // For std::unique_ptr
#include <ctime>
#include <sstream>	// For stringstream
#include "Snapshot.hpp"
#include <algorithm>

#define MAX_HOSTNAME_LEN 128
#define MAX_LOGMSG_LEN 256


char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
	char ** itr = std::find(begin, end, option);
	if (itr != end && ++itr != end)
	{
		return *itr;
	}
	return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
	return std::find(begin, end, option) != end;
}

void PrintErr(struct soap* _psoap)
{
	fflush(stdout);
	processEventLog(__FILE__, __LINE__, stdout, "error:%d faultstring:%s faultcode:%s faultsubcode:%s faultdetail:%s", _psoap->error,
	*soap_faultstring(_psoap), *soap_faultcode(_psoap),*soap_faultsubcode(_psoap), *soap_faultdetail(_psoap));
}

int main(int argc, char* argv[])
{

	char szHostName[MAX_HOSTNAME_LEN] = { 0 };

	// Proxy declarations
	DeviceBindingProxy proxyDevice;
	RemoteDiscoveryBindingProxy proxyDiscovery;
	MediaBindingProxy proxyMedia;

	if(!(  cmdOptionExists(argv, argv+argc, "-cIp")
			&& cmdOptionExists(argv, argv+argc, "-cUsr")
			&& cmdOptionExists(argv, argv+argc, "-cPwd")
#ifdef WITH_FTP_UPLOAD
			&& cmdOptionExists(argv, argv+argc, "-fIp")
			&& cmdOptionExists(argv, argv+argc, "-fUsr")
			&& cmdOptionExists(argv, argv+argc, "-fPwd")
#endif
			))
	{
		std::cout  <<  "usage: ./ipconvif -cIp [<camera-ip>:<port>] -cUsr <cam-id> -cPwd <cam-pwd>\n";

#ifdef WITH_FTP_UPLOAD
		std::cout  <<  "-fIp [<ftp-server-ip>:<port>] -fUsr <ftp-id> -fPwd <ftp-pwd>\n";
#endif

		return -1;
	}

	char *camIp = getCmdOption(argv, argv+argc, "-cIp");
	char *camUsr = getCmdOption(argv, argv+argc, "-cUsr");
	char *camPwd = getCmdOption(argv, argv+argc, "-cPwd");

#ifdef WITH_FTP_UPLOAD
	char *ftpIp = getCmdOption(argv, argv+argc, "-fIp");
	char *ftpUsr = getCmdOption(argv, argv+argc, "-fUsr");
	char *ftpPwd = getCmdOption(argv, argv+argc, "-fPwd");
#endif

	if (!(camIp && camUsr && camPwd
#ifdef WITH_FTP_UPLOAD
		&& ftpIp && ftpUsr && ftpPwd
#endif
		))
	{
		processEventLog(__FILE__, __LINE__, stdout, "Error: Invalid args (All args are required!)");
		return -1;
	}

	strcat(szHostName, "http://");
	strcat(szHostName, camIp);
	strcat(szHostName, "/onvif/device_service");

	proxyDevice.soap_endpoint = szHostName;

	// Register plugins
	soap_register_plugin(proxyDevice.soap, soap_wsse);
	soap_register_plugin(proxyDiscovery.soap, soap_wsse);
	soap_register_plugin(proxyMedia.soap, soap_wsse);

	struct soap *soap = soap_new();
	// For DeviceBindingProxy
	if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(proxyDevice.soap, NULL, camUsr, camPwd))
	{
		return -1;
	}

	if (SOAP_OK != soap_wsse_add_Timestamp(proxyDevice.soap, "Time", 10))
	{
		return -1;
	}

	// Get Device info
	_tds__GetDeviceInformation *tds__GetDeviceInformation = soap_new__tds__GetDeviceInformation(soap, -1);
	_tds__GetDeviceInformationResponse *tds__GetDeviceInformationResponse = soap_new__tds__GetDeviceInformationResponse(soap, -1);

	if (SOAP_OK == proxyDevice.GetDeviceInformation(tds__GetDeviceInformation, tds__GetDeviceInformationResponse))
	{
		processEventLog(__FILE__, __LINE__, stdout, "-------------------DeviceInformation-------------------");
		processEventLog(__FILE__, __LINE__, stdout, "\r\nManufacturer:%s\r\nModel:%s\r\nFirmwareVersion:%s\r\nSerialNumber:%s\r\nHardwareId:%s", tds__GetDeviceInformationResponse->Manufacturer.c_str(),
						tds__GetDeviceInformationResponse->Model.c_str(), tds__GetDeviceInformationResponse->FirmwareVersion.c_str(),
						tds__GetDeviceInformationResponse->SerialNumber.c_str(), tds__GetDeviceInformationResponse->HardwareId.c_str());
	}
	else
	{
		PrintErr(proxyDevice.soap);
		return -1;
	}

	// DeviceBindingProxy ends
	soap_destroy(soap);
	soap_end(soap);

	// Get Device capabilities
	_tds__GetCapabilities *tds__GetCapabilities = soap_new__tds__GetCapabilities(soap, -1);
	tds__GetCapabilities->Category.push_back(tt__CapabilityCategory__All);
	_tds__GetCapabilitiesResponse *tds__GetCapabilitiesResponse = soap_new__tds__GetCapabilitiesResponse(soap, -1);

printf("\n\n\n");
	processEventLog(__FILE__, __LINE__, stdout, "-------------------Capabilities-------------------");
	if (SOAP_OK == proxyDevice.GetCapabilities(tds__GetCapabilities, tds__GetCapabilitiesResponse))
	{
printf("\n");
		if (tds__GetCapabilitiesResponse->Capabilities->Analytics != NULL)
		{
			processEventLog(__FILE__, __LINE__, stdout, "-------------------Analytics-------------------");
			processEventLog(__FILE__, __LINE__, stdout, "XAddr:%s", tds__GetCapabilitiesResponse->Capabilities->Analytics->XAddr.c_str());
			processEventLog(__FILE__, __LINE__, stdout, "RuleSupport:%s", (tds__GetCapabilitiesResponse->Capabilities->Analytics->RuleSupport) ? "Y" : "N");
			processEventLog(__FILE__, __LINE__, stdout, "AnalyticsModuleSupport:%s", (tds__GetCapabilitiesResponse->Capabilities->Analytics->AnalyticsModuleSupport) ? "Y" : "N");
			processEventLog(__FILE__, __LINE__, stdout, "__anyAttribute:%s", tds__GetCapabilitiesResponse->Capabilities->Analytics->__anyAttribute);
		}

printf("\n");
		if (tds__GetCapabilitiesResponse->Capabilities->Device != NULL)
		{
			processEventLog(__FILE__, __LINE__, stdout, "-------------------Device-------------------");
			processEventLog(__FILE__, __LINE__, stdout, "XAddr:%s", tds__GetCapabilitiesResponse->Capabilities->Events->XAddr.c_str());
			if (tds__GetCapabilitiesResponse->Capabilities->Device->Network != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------Network-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "IPFilter:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Network->IPFilter) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "ZeroConfiguration:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Network->ZeroConfiguration) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "IPVersion6:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Network->IPVersion6) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "DynDNS:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Network->DynDNS) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "__anyAttribute:%s", tds__GetCapabilitiesResponse->Capabilities->Device->Network->__anyAttribute);
				if (tds__GetCapabilitiesResponse->Capabilities->Device->Network->Extension != NULL)
				{
					processEventLog(__FILE__, __LINE__, stdout, "-------------------Extension-------------------");
					processEventLog(__FILE__, __LINE__, stdout, "Dot11Configuration:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Network->Extension->Dot11Configuration) ? "Y" : "N");
					if (tds__GetCapabilitiesResponse->Capabilities->Device->Network->Extension->Extension != NULL)
					{
						processEventLog(__FILE__, __LINE__, stdout, "-------------------Extension2-------------------");
					}
				}
			}

			if (tds__GetCapabilitiesResponse->Capabilities->Device->System != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------System-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "DiscoveryResolve:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->System->DiscoveryResolve) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "DiscoveryBye:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->System->DiscoveryBye) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "RemoteDiscovery:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->System->RemoteDiscovery) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "SystemBackup:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->System->SystemBackup) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "SystemLogging:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->System->SystemLogging) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "FirmwareUpgrade:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->System->FirmwareUpgrade) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "__anyAttribute:%s", tds__GetCapabilitiesResponse->Capabilities->Device->System->__anyAttribute);
				if (tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension != NULL)
				{
					processEventLog(__FILE__, __LINE__, stdout, "-------------------Extension-------------------");
					processEventLog(__FILE__, __LINE__, stdout, "HttpFirmwareUpgrade:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpFirmwareUpgrade) ? "Y" : "N");
					processEventLog(__FILE__, __LINE__, stdout, "HttpSystemBackup:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpSystemBackup) ? "Y" : "N");
					processEventLog(__FILE__, __LINE__, stdout, "HttpSystemLogging:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpSystemLogging) ? "Y" : "N");
					processEventLog(__FILE__, __LINE__, stdout, "HttpSupportInformation:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->HttpSupportInformation) ? "Y" : "N");
					if (tds__GetCapabilitiesResponse->Capabilities->Device->System->Extension->Extension != NULL)
					{
						processEventLog(__FILE__, __LINE__, stdout, "-------------------Extension2-------------------");
					}
				}
			}

			if (tds__GetCapabilitiesResponse->Capabilities->Device->IO != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------IO-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "InputConnectors:%d", *tds__GetCapabilitiesResponse->Capabilities->Device->IO->InputConnectors);
				processEventLog(__FILE__, __LINE__, stdout, "RelayOutputs:%d", *tds__GetCapabilitiesResponse->Capabilities->Device->IO->RelayOutputs);
				processEventLog(__FILE__, __LINE__, stdout, "__anyAttribute:%s", tds__GetCapabilitiesResponse->Capabilities->Device->IO->__anyAttribute);
				if (tds__GetCapabilitiesResponse->Capabilities->Device->IO->Extension != NULL)
				{
					processEventLog(__FILE__, __LINE__, stdout, "-------------------Extension-------------------");
					processEventLog(__FILE__, __LINE__, stdout, "Auxiliary:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->IO->Extension->Auxiliary) ? "Y" : "N");
					processEventLog(__FILE__, __LINE__, stdout, "__anyAttribute:%s", tds__GetCapabilitiesResponse->Capabilities->Device->IO->Extension->__anyAttribute);
					if (tds__GetCapabilitiesResponse->Capabilities->Device->IO->Extension->Extension != NULL)
					{
						processEventLog(__FILE__, __LINE__, stdout, "-------------------Extension2-------------------");
					}
				}
			}

			if (tds__GetCapabilitiesResponse->Capabilities->Device->Security != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------Security-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "TLS1_x002e1:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Security->TLS1_x002e1) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "TLS1_x002e2:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Security->TLS1_x002e2) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "OnboardKeyGeneration:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Security->OnboardKeyGeneration) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "AccessPolicyConfig:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Security->AccessPolicyConfig) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "X_x002e509Token:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Security->X_x002e509Token) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "SAMLToken:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Security->SAMLToken) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "KerberosToken:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Security->KerberosToken) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "RELToken:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Security->RELToken) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "__anyAttribute:%s", tds__GetCapabilitiesResponse->Capabilities->Device->Security->__anyAttribute);
				if (tds__GetCapabilitiesResponse->Capabilities->Device->Security->Extension != NULL)
				{
					processEventLog(__FILE__, __LINE__, stdout, "-------------------Extension-------------------");
					processEventLog(__FILE__, __LINE__, stdout, "TLS1_x002e0:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Security->Extension->TLS1_x002e0) ? "Y" : "N");
					if (tds__GetCapabilitiesResponse->Capabilities->Device->Security->Extension->Extension != NULL)
					{
						processEventLog(__FILE__, __LINE__, stdout, "-------------------Extension2-------------------");
						processEventLog(__FILE__, __LINE__, stdout, "Dot1X:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Security->Extension->Extension->Dot1X) ? "Y" : "N");
						processEventLog(__FILE__, __LINE__, stdout, "RemoteUserHandling:%s", (tds__GetCapabilitiesResponse->Capabilities->Device->Security->Extension->Extension->RemoteUserHandling) ? "Y" : "N");
					}
				}
			}
			if (tds__GetCapabilitiesResponse->Capabilities->Device->Extension != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------Extension-------------------");
			}
		}

printf("\n");
		if (tds__GetCapabilitiesResponse->Capabilities->Events != NULL)
		{
			processEventLog(__FILE__, __LINE__, stdout, "-------------------Events-------------------");
			processEventLog(__FILE__, __LINE__, stdout, "XAddr:%s", tds__GetCapabilitiesResponse->Capabilities->Events->XAddr.c_str());
			processEventLog(__FILE__, __LINE__, stdout, "WSSubscriptionPolicySupport:%s", (tds__GetCapabilitiesResponse->Capabilities->Events->WSSubscriptionPolicySupport) ? "Y" : "N");
			processEventLog(__FILE__, __LINE__, stdout, "WSPullPointSupport:%s", (tds__GetCapabilitiesResponse->Capabilities->Events->WSPullPointSupport) ? "Y" : "N");
			processEventLog(__FILE__, __LINE__, stdout, "WSPausableSubscriptionManagerInterfaceSupport:%s", (tds__GetCapabilitiesResponse->Capabilities->Events->WSPausableSubscriptionManagerInterfaceSupport) ? "Y" : "N");
			processEventLog(__FILE__, __LINE__, stdout, "__anyAttribute:%s", tds__GetCapabilitiesResponse->Capabilities->Events->__anyAttribute);
		}

printf("\n");
		if (tds__GetCapabilitiesResponse->Capabilities->Imaging != NULL)
		{
			processEventLog(__FILE__, __LINE__, stdout, "-------------------Imaging-------------------");
			processEventLog(__FILE__, __LINE__, stdout, "XAddr:%s", tds__GetCapabilitiesResponse->Capabilities->Imaging->XAddr.c_str());
		}

printf("\n");
		if (tds__GetCapabilitiesResponse->Capabilities->Media != NULL)
		{
			processEventLog(__FILE__, __LINE__, stdout, "-------------------Media-------------------");
			processEventLog(__FILE__, __LINE__, stdout, "XAddr:%s", tds__GetCapabilitiesResponse->Capabilities->Media->XAddr.c_str());

			if (tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------streaming-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "RTPMulticast:%s", (tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTPMulticast) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "RTP_TCP:%s", (tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTP_USCORETCP) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "RTP_RTSP_TCP:%s", (tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTP_USCORERTSP_USCORETCP) ? "Y" : "N");
			}

			if (tds__GetCapabilitiesResponse->Capabilities->Media->Extension != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------Extension-------------------");
				if (tds__GetCapabilitiesResponse->Capabilities->Media->Extension->ProfileCapabilities != NULL)
				{
					processEventLog(__FILE__, __LINE__, stdout, "-------------------ProfileCapabilities-------------------");
					processEventLog(__FILE__, __LINE__, stdout, "MaximumNumberOfProfiles:%d", tds__GetCapabilitiesResponse->Capabilities->Media->Extension->ProfileCapabilities->MaximumNumberOfProfiles);
				}
			}

			proxyMedia.soap_endpoint = tds__GetCapabilitiesResponse->Capabilities->Media->XAddr.c_str();


			// TODO: Add check to see if device is capable of 'GetSnapshotUri', if not skip this device!
		}

printf("\n");
		if (tds__GetCapabilitiesResponse->Capabilities->PTZ != NULL)
		{
			processEventLog(__FILE__, __LINE__, stdout, "-------------------PTZ-------------------");
			processEventLog(__FILE__, __LINE__, stdout, "XAddr:%s", tds__GetCapabilitiesResponse->Capabilities->PTZ->XAddr.c_str());
		}
		else
		{
			processEventLog(__FILE__, __LINE__, stdout, "-------------------NO PTZ-------------------");
		}

printf("\n");
		if (tds__GetCapabilitiesResponse->Capabilities->Extension != NULL)
		{
			processEventLog(__FILE__, __LINE__, stdout, "-------------------Extension-------------------");

			if (tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------DeviceIO-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "VideoSources:%d", tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->VideoSources);
				processEventLog(__FILE__, __LINE__, stdout, "VideoOutputs:%d", tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->VideoOutputs);
				processEventLog(__FILE__, __LINE__, stdout, "AudioSources:%d", tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->AudioSources);
				processEventLog(__FILE__, __LINE__, stdout, "AudioOutputs:%d", tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->AudioOutputs);
				processEventLog(__FILE__, __LINE__, stdout, "RelayOutputs:%d", tds__GetCapabilitiesResponse->Capabilities->Extension->DeviceIO->RelayOutputs);
			}

			if (tds__GetCapabilitiesResponse->Capabilities->Extension->Display != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------Display-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "FixedLayout:%s", (tds__GetCapabilitiesResponse->Capabilities->Extension->Display->FixedLayout) ? "Y" : "N");
			}

			if (tds__GetCapabilitiesResponse->Capabilities->Extension->Recording != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------Recording-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "XAddr:%s", tds__GetCapabilitiesResponse->Capabilities->Extension->Recording->XAddr.c_str());
				processEventLog(__FILE__, __LINE__, stdout, "ReceiverSource:%s", (tds__GetCapabilitiesResponse->Capabilities->Extension->Recording->ReceiverSource) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "MediaProfileSource:%s", (tds__GetCapabilitiesResponse->Capabilities->Extension->Recording->MediaProfileSource) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "DynamicRecordings:%s", (tds__GetCapabilitiesResponse->Capabilities->Extension->Recording->DynamicRecordings) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "DynamicTracks:%s", (tds__GetCapabilitiesResponse->Capabilities->Extension->Recording->DynamicTracks) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "MaxStringLength:%d", tds__GetCapabilitiesResponse->Capabilities->Extension->Recording->MaxStringLength);
			}

			if (tds__GetCapabilitiesResponse->Capabilities->Extension->Search != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------Search-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "MetadataSearch:%s", (tds__GetCapabilitiesResponse->Capabilities->Extension->Search->MetadataSearch) ? "Y" : "N");
			}

			if (tds__GetCapabilitiesResponse->Capabilities->Extension->Replay != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------Replay-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "XAddr:%s", tds__GetCapabilitiesResponse->Capabilities->Extension->Replay->XAddr.c_str());
			}

			if (tds__GetCapabilitiesResponse->Capabilities->Extension->Receiver != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------Receiver-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "XAddr:%s", tds__GetCapabilitiesResponse->Capabilities->Extension->Receiver->XAddr.c_str());
				processEventLog(__FILE__, __LINE__, stdout, "RTP_USCOREMulticast:%s", (tds__GetCapabilitiesResponse->Capabilities->Extension->Receiver->RTP_USCOREMulticast) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "RTP_USCORETCP:%s", (tds__GetCapabilitiesResponse->Capabilities->Extension->Receiver->RTP_USCORETCP) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "RTP_USCORERTSP_USCORETCP:%s", (tds__GetCapabilitiesResponse->Capabilities->Extension->Receiver->RTP_USCORERTSP_USCORETCP) ? "Y" : "N");
				processEventLog(__FILE__, __LINE__, stdout, "SupportedReceivers:%d", tds__GetCapabilitiesResponse->Capabilities->Extension->Receiver->SupportedReceivers);
				processEventLog(__FILE__, __LINE__, stdout, "MaximumRTSPURILength:%d", tds__GetCapabilitiesResponse->Capabilities->Extension->Receiver->MaximumRTSPURILength);
			}

			if (tds__GetCapabilitiesResponse->Capabilities->Extension->AnalyticsDevice != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------AnalyticsDevice-------------------");
				processEventLog(__FILE__, __LINE__, stdout, "XAddr:%s", tds__GetCapabilitiesResponse->Capabilities->Extension->AnalyticsDevice->XAddr.c_str());
				processEventLog(__FILE__, __LINE__, stdout, "RuleSupport:%s", (tds__GetCapabilitiesResponse->Capabilities->Extension->AnalyticsDevice->RuleSupport) ? "Y" : "N");
				if (tds__GetCapabilitiesResponse->Capabilities->Extension->AnalyticsDevice->Extension != NULL)
				{
					processEventLog(__FILE__, __LINE__, stdout, "-------------------AnalyticsDevice-------------------");
				}
			}

			if (tds__GetCapabilitiesResponse->Capabilities->Extension->Extensions != NULL)
			{
				processEventLog(__FILE__, __LINE__, stdout, "-------------------Extensions2-------------------");
			}
		}
		else
		{
			processEventLog(__FILE__, __LINE__, stdout, "-------------------NO Extension-------------------");
		}
	}

printf("\n\n\n");
	// For MediaBindingProxy
	if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(proxyMedia.soap, NULL, camUsr, camPwd))
	{
		return -1;
	}

	if (SOAP_OK != soap_wsse_add_Timestamp(proxyMedia.soap, "Time", 10))
	{
		return -1;
	}

	// Get Device Profiles
	_trt__GetProfiles *trt__GetProfiles = soap_new__trt__GetProfiles(soap, -1);
	_trt__GetProfilesResponse *trt__GetProfilesResponse = soap_new__trt__GetProfilesResponse(soap, -1);


	if (SOAP_OK == proxyMedia.GetProfiles(trt__GetProfiles, trt__GetProfilesResponse))
	{
		_trt__GetSnapshotUriResponse *trt__GetSnapshotUriResponse = soap_new__trt__GetSnapshotUriResponse(soap, -1);
		_trt__GetSnapshotUri *trt__GetSnapshotUri = soap_new__trt__GetSnapshotUri(soap, -1);

		// processEventLog(__FILE__, __LINE__, stdout, "-------------------ONE SNAPSHOT PER PROFILE-------------------");

		int n = trt__GetProfilesResponse->Profiles.size();
		processEventLog(__FILE__, __LINE__, stdout, "Profiles.size:%d",n);
		// Loop for every profile
		for (int i = 0; i < n; i++)
		{
			printf("Error %s %d i:%d\n",__FUNCTION__,__LINE__,i);
			trt__GetSnapshotUri->ProfileToken = trt__GetProfilesResponse->Profiles[i]->token;

			if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(proxyMedia.soap, NULL, camUsr, camPwd))
			{
				printf("Error %s %d",__FUNCTION__,__LINE__);
				PrintErr(proxyMedia.soap);
				return -1;
			}

			// Get Snapshot URI for profile
			if (SOAP_OK == proxyMedia.GetSnapshotUri(trt__GetSnapshotUri, trt__GetSnapshotUriResponse))
			{
				processEventLog(__FILE__, __LINE__, stdout, "[%d] Profile Name- %s", i, trt__GetProfilesResponse->Profiles[i]->Name.c_str());
				processEventLog(__FILE__, __LINE__, stdout, "[%d] SNAPSHOT URI- %s", i, trt__GetSnapshotUriResponse->MediaUri->Uri.c_str());

				//date  string
				char dateString[1024];
				time_t now = time(0);
				tm *ltm = localtime(&now);
				sprintf(dateString,"%04d_%02d_%02d_%02d_%02d_%02d.jpg",ltm->tm_year+1900,ltm->tm_mon+1,ltm->tm_mday,ltm->tm_hour,ltm->tm_min,ltm->tm_sec);

				// Create a 'Snapshot' instance
				Snapshot *snapshot = new Snapshot("./downloads", dateString);

				// Download snapshot file locally
				CURLcode result = snapshot->download(trt__GetSnapshotUriResponse->MediaUri->Uri);

#ifdef WITH_FTP_UPLOAD
				if(CURLE_OK == result) {
					std::string upload = snapshot->getUploadUri(gateName, ftpIp, ftpUsr, ftpPwd);
					processEventLog(__FILE__, __LINE__, stdout, "Uploading using: %s", upload.c_str());
					system(upload.c_str());
					sleep(5);
				}
#endif

				// Delete current 'Snapshot' instance
				// delete snapshot;
			}
			else
			{
				PrintErr(proxyMedia.soap);
			}
		}
	}
	else
	{
		PrintErr(proxyMedia.soap);
	}

	// MediaBindingProxy ends
	soap_destroy(soap);
	soap_end(soap);

	return 0;
}


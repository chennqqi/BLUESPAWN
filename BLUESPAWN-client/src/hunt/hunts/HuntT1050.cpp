#include "hunt/hunts/HuntT1050.h"

#include "util/eventlogs/EventLogs.h"
#include "util/log/Log.h"
#include "util/log/HuntLogMessage.h"
#include "util/filesystem/YaraScanner.h"
#include "util/processes/ProcessUtils.h"
#include "util/processes/CheckLolbin.h"

#include "common/Utils.h"

#include <map>

namespace Hunts {

	HuntT1050::HuntT1050() : Hunt(L"T1050 - New Service") {
		// TODO: update these categories
		dwSupportedScans = (DWORD) Aggressiveness::Normal | (DWORD) Aggressiveness::Intensive;
		dwCategoriesAffected = (DWORD) Category::Configurations | (DWORD) Category::Files;
		dwSourcesInvolved = (DWORD) DataSource::Registry | (DWORD) DataSource::FileSystem;
		dwTacticsUsed = (DWORD) Tactic::Persistence;
	}

	std::vector<EventLogs::EventLogItem> HuntT1050::Get7045Events() {
		// Create existance queries so interesting data is output
		std::vector<EventLogs::XpathQuery> queries;
		auto param1 = EventLogs::ParamList();
		auto param2 = EventLogs::ParamList();
		auto param3 = EventLogs::ParamList();
		auto param4 = EventLogs::ParamList();
		param1.push_back(std::make_pair(L"Name", L"'ServiceName'"));
		param2.push_back(std::make_pair(L"Name", L"'ImagePath'"));
		param3.push_back(std::make_pair(L"Name", L"'ServiceType'"));
		param4.push_back(std::make_pair(L"Name", L"'StartType'"));
		queries.push_back(EventLogs::XpathQuery(L"Event/EventData/Data", param1));
		queries.push_back(EventLogs::XpathQuery(L"Event/EventData/Data", param2));
		queries.push_back(EventLogs::XpathQuery(L"Event/EventData/Data", param3));
		queries.push_back(EventLogs::XpathQuery(L"Event/EventData/Data", param4));

		auto queryResults = EventLogs::QueryEvents(L"System", 7045, queries);

		return queryResults;
	}

	int HuntT1050::ScanNormal(const Scope& scope, Reaction reaction) {
		LOG_INFO(L"Hunting for " << name << L" at level Normal");
		reaction.BeginHunt(GET_INFO());

		auto queryResults = Get7045Events();

		auto& yara = YaraScanner::GetInstance();
		int detections = 0;
		
		std::map<std::pair<std::wstring, std::wstring>, bool> findings{};
		
		for(auto result : queryResults){
			auto imageName = result.GetProperty(L"Event/EventData/Data[@Name='ServiceName']");

			auto cmd{ result.GetProperty(L"Event/EventData/Data[@Name='ImagePath']") };
			std::pair<std::wstring, std::wstring> pair{ imageName, cmd };

			if(findings.count(pair)){
				if(findings.at(pair)){
					reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));
				}
			} else{
				if(IsLolbinMalicious(cmd)){
					reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));
					findings.emplace(pair, true);
					detections++;
				} else{
					auto imagePath = GetImagePathFromCommand(cmd);

					FileSystem::File file = FileSystem::File(imagePath);
					if(file.GetFileExists() && !file.GetFileSigned()){
						reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));

						auto& yara = YaraScanner::GetInstance();
						YaraScanResult result = yara.ScanFile(file);

						reaction.FileIdentified(std::make_shared<FILE_DETECTION>(file));

						detections += 2;
						findings.emplace(pair, true);
					}

					// Look for PSExec services
					else if(imageName.find(L"PSEXESVC") != std::wstring::npos){
						reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));
						detections++;
						findings.emplace(pair, true);
					}

					// Look for Mimikatz Driver loading
					else if(imageName.find(L"mimikatz") != std::wstring::npos || imageName.find(L"mimidrv") != std::wstring::npos
					   || imagePath.find(L"mimidrv.sys") != std::wstring::npos){
						reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));
						reaction.FileIdentified(std::make_shared<FILE_DETECTION>(file));
						detections += 2;
						findings.emplace(pair, true);
					}

					// Calculate entropy of service names to look for suspicious services like 
					// the ones MSF generates https://www.offensive-security.com/metasploit-unleashed/psexec-pass-hash/
					else if(false && (GetShannonEntropy(imageName) < 3.00 || GetShannonEntropy(imageName) > 5.00)){
						reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));
						reaction.FileIdentified(std::make_shared<FILE_DETECTION>(file));
						detections += 2;
						findings.emplace(pair, true);
					} else {
						findings.emplace(pair, false);
					}
				}
			}
		}

		reaction.EndHunt();
		return detections;
	}

	int HuntT1050::ScanIntensive(const Scope& scope, Reaction reaction){
		LOG_INFO(L"Hunting for " << name << L" at level Intensive");
		reaction.BeginHunt(GET_INFO());

		auto queryResults = Get7045Events();

		auto& yara = YaraScanner::GetInstance();
		int detections = 0;

		std::map<std::pair<std::wstring, std::wstring>, bool> findings{};

		for(auto result : queryResults){
			auto imageName = result.GetProperty(L"Event/EventData/Data[@Name='ServiceName']");

			auto cmd{ result.GetProperty(L"Event/EventData/Data[@Name='ImagePath']") };
			std::pair<std::wstring, std::wstring> pair{ imageName, cmd };

			if(findings.count(pair)){
				if(findings.at(pair)){
					reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));
				}
			} else{
				if(IsLolbinMalicious(cmd)){
					reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));
					findings.emplace(pair, true);
					detections++;
				} else{
					auto imagePath = GetImagePathFromCommand(cmd);

					FileSystem::File file = FileSystem::File(imagePath);
					if(file.GetFileExists() && !file.GetFileSigned()){
						reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));

						auto& yara = YaraScanner::GetInstance();
						YaraScanResult result = yara.ScanFile(file);

						reaction.FileIdentified(std::make_shared<FILE_DETECTION>(file));

						detections += 2;
						findings.emplace(pair, true);
					}

					// Look for PSExec services
					else if(imageName.find(L"PSEXESVC") != std::wstring::npos){
						reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));
						detections++;
						findings.emplace(pair, true);
					}

					// Look for Mimikatz Driver loading
					else if(imageName.find(L"mimikatz") != std::wstring::npos || imageName.find(L"mimidrv") != std::wstring::npos
							|| imagePath.find(L"mimidrv.sys") != std::wstring::npos){
						reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));
						reaction.FileIdentified(std::make_shared<FILE_DETECTION>(file));
						detections += 2;
						findings.emplace(pair, true);
					}

					// Calculate entropy of service names to look for suspicious services like 
					// the ones MSF generates https://www.offensive-security.com/metasploit-unleashed/psexec-pass-hash/
					else if(!file.GetFileExists() || (GetShannonEntropy(imageName) < 3.00 || GetShannonEntropy(imageName) > 5.00)){
						reaction.EventIdentified(EventLogs::EventLogItemToDetection(result));
						reaction.FileIdentified(std::make_shared<FILE_DETECTION>(file));
						detections += 2;
						findings.emplace(pair, true);
					} else{
						findings.emplace(pair, false);
					}
				}
			}
		}

		reaction.EndHunt();
		return detections;

	}

	std::vector<std::shared_ptr<Event>> HuntT1050::GetMonitoringEvents() {
		std::vector<std::shared_ptr<Event>> events;

		events.push_back(std::make_shared<EventLogEvent>(L"System", 7045));
		
		return events;
	}

}

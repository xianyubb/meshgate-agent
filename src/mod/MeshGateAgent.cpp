#include "mod/MeshGateAgent.h"

#include "agent/HttpClient.h"
#include "agent/StringUtil.h"

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/service/Bedrock.h"

#include "mc/certificates/identity/PlayerAuthenticationInfo.h"
#include "mc/certificates/identity/PlayerAuthenticationType.h"
#include "mc/common/Globals.h"
#include "mc/deps/certificates/WebToken.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/deps/ecs/gamerefs_entity/GameRefsEntity.h"
#include "mc/network/ConnectionRequest.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/network/packet/LoginPacket.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace meshgate_agent {
namespace {

constexpr auto kDefaultConfig = R"({
  "apiHost": "127.0.0.1",
  "apiPort": 31920,
  "bearerToken": "replace-with-the-same-token-as-meshgate",
  "command": "meshgate"
}
)";

} // namespace

struct TransferParams {
  std::string target;
};

constexpr std::string_view kMeshGateXuidModel = "MeshGate-XUID:";
constexpr std::string_view kMeshGateXuidDevice = "meshgate:xuid:";

bool equalsIgnoreCase(std::string_view left, std::string_view right) {
  if (left.size() != right.size())
    return false;
  for (std::size_t i = 0; i < left.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(left[i])) !=
        std::tolower(static_cast<unsigned char>(right[i])))
      return false;
  }
  return true;
}

std::string decodeBase64Url(std::string_view input) {
  std::string output;
  output.reserve(input.size() * 3 / 4);
  int value = 0;
  int bits = -8;
  for (const auto ch : input) {
    if (ch == '=')
      break;
    int digit = -1;
    if (ch >= 'A' && ch <= 'Z')
      digit = ch - 'A';
    else if (ch >= 'a' && ch <= 'z')
      digit = ch - 'a' + 26;
    else if (ch >= '0' && ch <= '9')
      digit = ch - '0' + 52;
    else if (ch == '-')
      digit = 62;
    else if (ch == '_')
      digit = 63;
    else
      continue;
    value = (value << 6) | digit;
    bits += 6;
    if (bits >= 0) {
      output.push_back(static_cast<char>((value >> bits) & 0xff));
      bits -= 8;
    }
  }
  return output;
}

std::string jsonStringField(std::string_view json, std::string_view field) {
  const auto key = std::string{"\""} + std::string{field} + "\"";
  const auto keyPos = json.find(key);
  if (keyPos == std::string_view::npos)
    return {};
  const auto colon = json.find(':', keyPos + key.size());
  if (colon == std::string_view::npos)
    return {};
  const auto firstQuote = json.find('"', colon + 1);
  if (firstQuote == std::string_view::npos)
    return {};

  std::string result;
  bool escaped = false;
  for (std::size_t i = firstQuote + 1; i < json.size(); ++i) {
    const auto ch = json[i];
    if (escaped) {
      result.push_back(ch);
      escaped = false;
    } else if (ch == '\\') {
      escaped = true;
    } else if (ch == '"') {
      return result;
    } else {
      result.push_back(ch);
    }
  }
  return {};
}

std::string jsonValueField(Json::Value const &value, std::string_view field,
                           int depth = 0) {
  if (depth > 8)
    return {};
  if (value.isObject()) {
    for (const auto &member : value.getMemberNames()) {
      const auto &child = value[member];
      if (equalsIgnoreCase(member, field) && child.isString()) {
        const auto result = child.asString("");
        if (!result.empty())
          return result;
      }
      if (const auto result = jsonValueField(child, field, depth + 1);
          !result.empty()) {
        return result;
      }
    }
  } else if (value.isArray()) {
    for (Json::Value::ArrayIndex i = 0; i < value.size(); ++i) {
      if (const auto result = jsonValueField(value[i], field, depth + 1);
          !result.empty()) {
        return result;
      }
    }
  }
  return {};
}

std::string connectionField(ConnectionRequest const &request,
                            std::string_view field) {
  if (equalsIgnoreCase(field, "DeviceId"))
    return request.getDeviceId();
  if (equalsIgnoreCase(field, "DeviceModel"))
    return request.getDeviceModel();
  if (equalsIgnoreCase(field, "ThirdPartyName"))
    return request.getThirdPartyName();
  if (equalsIgnoreCase(field, "PlatformOnlineId"))
    return request.getPlatformOnlineId();
  if (equalsIgnoreCase(field, "PlatformOfflineId"))
    return request.getPlatformOfflineId();
  if (equalsIgnoreCase(field, "SelfSignedId"))
    return request.getSelfSignedId();

  const auto &rawToken = request.mRawToken.get();
  if (!rawToken)
    return {};
  for (const auto *json : {&rawToken->mDataInfo, &rawToken->mHeaderInfo}) {
    if (const auto result = jsonValueField(*json, field); !result.empty())
      return result;
  }
  const auto rawTokenString = rawToken->toString();
  for (const auto source :
       {std::string_view{rawToken->mData}, std::string_view{rawTokenString}}) {
    if (const auto result = jsonStringField(source, field); !result.empty())
      return result;
    if (const auto result = jsonStringField(decodeBase64Url(source), field);
        !result.empty()) {
      return result;
    }
  }
  return {};
}

std::string carrierValue(std::string_view value) {
  if (value.starts_with(kMeshGateXuidModel)) {
    return std::string{value.substr(kMeshGateXuidModel.size())};
  }
  if (value.starts_with(kMeshGateXuidDevice)) {
    return std::string{value.substr(kMeshGateXuidDevice.size())};
  }
  return {};
}

std::string findXuid(ConnectionRequest const &request) {
  for (const auto field : {std::string_view{"xid"}, std::string_view{"xuid"},
                           std::string_view{"XUID"}}) {
    if (const auto value = connectionField(request, field); !value.empty())
      return value;
  }
  for (const auto field :
       {std::string_view{"DeviceModel"}, std::string_view{"DeviceId"}}) {
    if (const auto value = carrierValue(connectionField(request, field));
        !value.empty()) {
      return value;
    }
  }
  return {};
}

std::string firstField(ConnectionRequest const &request,
                       std::initializer_list<std::string_view> fields) {
  for (const auto field : fields) {
    if (const auto value = connectionField(request, field); !value.empty())
      return value;
  }
  return {};
}

bool injectPlayerIdentity(ConnectionRequest const &request,
                          PlayerAuthenticationInfo &info) {
  const auto xuid = findXuid(request);
  if (xuid.empty())
    return false;

  info.Xuid = xuid;
  info.PlayFabId = firstField(request, {"mid", "PlayFabId"});
  info.XboxLiveName = firstField(request, {"xname", "ThirdPartyName"});
  info.PublicKey = firstField(request, {"cpk", "x5u"});
  info.AuthenticatedUuid = makePlayerUUIDForXUID(xuid);
  return true;
}

void logConnectionShape(ConnectionRequest const &request) {
  const auto &rawToken = request.mRawToken.get();
  std::cerr << "[meshgate-agent] identity carriers: DeviceId="
            << request.getDeviceId()
            << ", DeviceModel=" << request.getDeviceModel()
            << ", SelfSignedId=" << request.getSelfSignedId();
  if (rawToken) {
    std::cerr << ", fields=";
    const auto fields = rawToken->mDataInfo.getMemberNames();
    for (std::size_t i = 0; i < fields.size(); ++i) {
      if (i != 0)
        std::cerr << ',';
      std::cerr << fields[i];
    }
  }
  std::cerr << '\n';
}

void logIdentity(PlayerAuthenticationInfo const &info, char const *stage) {
  std::cerr << "[meshgate-agent] injected identity at " << stage
            << ": xuid=" << *info.Xuid
            << ", uuid=" << (*info.AuthenticatedUuid).asString()
            << ", playFabId=" << *info.PlayFabId
            << ", xboxLiveName=" << *info.XboxLiveName << '\n';
}

LL_AUTO_TYPE_INSTANCE_HOOK(MeshGateValidateLoginIdentityHook,
                           HookPriority::Normal, ServerNetworkHandler,
                           &ServerNetworkHandler::$_validateLoginPacket,
                           std::optional<PlayerAuthenticationInfo>,
                           NetworkIdentifier const &source,
                           LoginPacket const &packet) {
  auto validated = origin(source, packet);
  const auto &request = packet.mConnectionRequest.get();
  if (!validated || !request) {
    std::cerr
        << "[meshgate-agent] BDS rejected Login before identity injection\n";
    return validated;
  }
  if (injectPlayerIdentity(*request, *validated)) {
    const_cast<ConnectionRequest &>(*request).mAuthenticationType =
        PlayerAuthenticationType::Full;
    logIdentity(*validated, "_validateLoginPacket");
  } else {
    std::cerr << "[meshgate-agent] no MeshGate XUID carrier in Login\n";
    logConnectionShape(*request);
  }
  return validated;
}

LL_AUTO_TYPE_INSTANCE_HOOK(MeshGateCreatePlayerIdentityHook,
                           HookPriority::Normal, ServerNetworkHandler,
                           &ServerNetworkHandler::createNewPlayer,
                           OwnerPtr<EntityContext>,
                           NetworkIdentifier const &source,
                           ConnectionRequest const &connectionRequest,
                           PlayerAuthenticationInfo const &playerInfo) {
  auto enriched = playerInfo;
  if (!injectPlayerIdentity(connectionRequest, enriched)) {
    std::cerr
        << "[meshgate-agent] createNewPlayer found no MeshGate XUID carrier\n";
    return origin(source, connectionRequest, playerInfo);
  }
  logIdentity(enriched, "createNewPlayer");
  auto normalizedRequest = connectionRequest;
  normalizedRequest.mAuthenticationType = PlayerAuthenticationType::Full;
  return origin(source, normalizedRequest, enriched);
}

MeshGateAgent &MeshGateAgent::getInstance() {
  static MeshGateAgent instance;
  return instance;
}

bool MeshGateAgent::load() {
  auto &self = getSelf();
  std::filesystem::create_directories(self.getConfigDir());

  const auto configPath = self.getConfigDir() / "config.json";
  if (!std::filesystem::exists(configPath)) {
    std::ofstream output{configPath};
    output << kDefaultConfig;
    self.getLogger().warn(
        "created default config at {}; set bearerToken before using /meshgate",
        configPath.string());
  }

  mConfig = loadConfigFile(configPath);

  self.getLogger().info("meshgate-agent loaded; control API {}:{}",
                        mConfig.apiHost, mConfig.apiPort);
  if (mConfig.bearerToken == "replace-with-the-same-token-as-meshgate") {
    self.getLogger().warn(
        "meshgate-agent bearerToken is still the placeholder value");
  }
  return true;
}

bool MeshGateAgent::enable() {
  auto registry = ll::service::getCommandRegistry();
  if (!registry) {
    getSelf().getLogger().error("failed to get command registry");
    return false;
  }

  auto &command =
      ll::command::CommandRegistrar::getInstance(false).getOrCreateCommand(
          mConfig.command, "Transfer to a MeshGate backend",
          CommandPermissionLevel::Any);

  command.overload<TransferParams>().required("target").execute(
      [this](CommandOrigin const &origin, CommandOutput &output,
             TransferParams const &params) {
        auto *entity = origin.getEntity();
        if (entity == nullptr ||
            entity->getEntityTypeId() != ActorType::Player) {
          output.error("Only players can use this command");
          return;
        }

        auto *player = static_cast<Player *>(entity);
        auto xuid = player->getXuid();
        auto name = player->getName();

        std::string response;
        if (!postHandoff(mConfig, xuid, name, params.target, response)) {
          output.error("MeshGate handoff rejected: {}", trim(response));
          return;
        }

        output.success("MeshGate handoff accepted: {}", params.target);
      });

  getSelf().getLogger().info("registered /{}", mConfig.command);
  return true;
}

bool MeshGateAgent::disable() {
  getSelf().getLogger().info("meshgate-agent disabled");
  return true;
}

} // namespace meshgate_agent

LL_REGISTER_MOD(meshgate_agent::MeshGateAgent,
                meshgate_agent::MeshGateAgent::getInstance());

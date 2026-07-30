#include "mod/MeshGateAgent.h"

#include "agent/HttpClient.h"
#include "agent/StringUtil.h"

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/service/Bedrock.h"

#include "mc/server/DedicatedServer.h"
#include "mc/server/PropertiesSettings.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include <filesystem>
#include <fstream>
#include <string>

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

LL_AUTO_TYPE_INSTANCE_HOOK(PropertiesSettingsisClientSideGenEnabledHook,
                           HookPriority::Normal, DedicatedServer,
                           &DedicatedServer::initializeHttp, void,
                           PropertiesSettings const &properties) {
  auto &properties_modiy = const_cast<PropertiesSettings &>(properties);
  properties_modiy.mClientSideGenerationEnabled = false;
  return origin(properties_modiy);
}

LL_REGISTER_MOD(meshgate_agent::MeshGateAgent,
                meshgate_agent::MeshGateAgent::getInstance());

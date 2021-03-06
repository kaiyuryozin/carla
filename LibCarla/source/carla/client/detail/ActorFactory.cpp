// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "carla/client/detail/ActorFactory.h"

#include "carla/Logging.h"
#include "carla/StringUtil.h"
#include "carla/client/Actor.h"
#include "carla/client/Sensor.h"
#include "carla/client/TrafficLight.h"
#include "carla/client/Vehicle.h"
#include "carla/client/World.h"
#include "carla/client/detail/Client.h"

#include <rpc/config.h>
#include <rpc/rpc_error.h>

#include <exception>

namespace carla {
namespace client {
namespace detail {

  // A deleter cannot throw exceptions; and unlike std::unique_ptr, the deleter
  // of (std|boost)::shared_ptr is invoked even if the managed pointer is null.
  struct GarbageCollector {
    void operator()(::carla::client::Actor *ptr) const noexcept {
      if ((ptr != nullptr) && ptr->IsAlive()) {
        try {
          ptr->Destroy();
          delete ptr;
        } catch (const ::rpc::timeout &timeout) {
          log_error(timeout.what());
          log_error(
              "timeout while trying to garbage collect Actor",
              ptr->GetDisplayId(),
              "actor hasn't been removed from the simulation");
        } catch (const std::exception &e) {
          log_critical(
              "exception thrown while trying to garbage collect Actor",
              ptr->GetDisplayId(),
              e.what());
          throw; // calls terminate.
        } catch (...) {
          log_critical(
              "unknown exception thrown while trying to garbage collect an Actor :",
              ptr->GetDisplayId());
          throw; // calls terminate.
        }
      }
    }
  };

  template <typename ActorT>
  static auto MakeActorImpl(ActorInitializer init, GarbageCollectionPolicy gc) {
    if (gc == GarbageCollectionPolicy::Enabled) {
      return SharedPtr<ActorT>{new ActorT(std::move(init)), GarbageCollector()};
    }
    DEBUG_ASSERT(gc == GarbageCollectionPolicy::Disabled);
    return SharedPtr<ActorT>{new ActorT(std::move(init))};
  }

  SharedPtr<Actor> ActorFactory::MakeActor(
      EpisodeProxy episode,
      rpc::Actor description,
      GarbageCollectionPolicy gc) {
    if (description.HasAStream()) {
      return MakeActorImpl<Sensor>(ActorInitializer{description, episode}, gc);
    } else if (StringUtil::StartsWith(description.description.id, "vehicle.")) {
      return MakeActorImpl<Vehicle>(ActorInitializer{description, episode}, gc);
    } else if (StringUtil::StartsWith(description.description.id, "traffic.traffic_light")) {
      return MakeActorImpl<TrafficLight>(ActorInitializer{description, episode}, gc);
    }
    return MakeActorImpl<Actor>(ActorInitializer{description, episode}, gc);
  }

} // namespace detail
} // namespace client
} // namespace carla

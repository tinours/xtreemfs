/*
 * Copyright (c)  2009 Juan Gonzalez de Benito,
 *                2011 Bjoern Kolbeck (Zuse Institute Berlin),
 *                2012 Matthias Noack (Zuse Institute Berlin)
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */

#include "libxtreemfs/vivaldi.h"

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/lexical_cast.hpp>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "libxtreemfs/callback/execute_sync_request.h"
#include "libxtreemfs/options.h"
#include "libxtreemfs/pbrpc_url.h"
#include "util/logging.h"
#include "util/zipf_generator.h"
#include "xtreemfs/GlobalTypes.pb.h"
#include "xtreemfs/OSDServiceClient.h"

using namespace xtreemfs::pbrpc;
using namespace xtreemfs::util;

namespace xtreemfs {

Vivaldi::Vivaldi(
    xtreemfs::rpc::Client* rpc_client,
    DIRServiceClient* dir_client,
    UUIDIterator* dir_service_addresses,
    UUIDResolver* uuid_resolver,
    const Options& options)
  : rpc_client_(rpc_client),
    dir_client_(dir_client),
    dir_service_addresses_(dir_service_addresses),
    uuid_resolver_(uuid_resolver),
    options_(options) {
  srand(static_cast<unsigned int>(time(NULL)));
  // Set AuthType to AUTH_NONE as it's currently not used.
  auth_bogus_.set_auth_type(AUTH_NONE);
  // Set username "xtreemfs" as it does not get checked at server side.
  user_credentials_bogus_.set_username("xtreemfs");
}

void Vivaldi::Run() {
  // Initialized to (0,0) by default
  my_vivaldi_coordinates_.set_local_error(0.0);
  my_vivaldi_coordinates_.set_x_coordinate(0.0);
  my_vivaldi_coordinates_.set_y_coordinate(0.0);

  std::ifstream vivaldi_coordinates_file(options_.vivaldi_filename.c_str());
  if (vivaldi_coordinates_file.is_open()) {
    my_vivaldi_coordinates_.ParseFromIstream(&vivaldi_coordinates_file);
    if (!my_vivaldi_coordinates_.IsInitialized()) {
        Logging::log->getLog(LEVEL_ERROR)
            << "Vivaldi: Could not load coordinates from file: "
            << my_vivaldi_coordinates_.InitializationErrorString() << std::endl;
      my_vivaldi_coordinates_.Clear();
    }
  } else {
    if (Logging::log->loggingActive(LEVEL_INFO)) {
      Logging::log->getLog(LEVEL_INFO)
          << "Vivaldi: Coordinates file does not exist,"
          << "starting with empty coordinates" << std::endl
          << "Initialization might take some time." << std::endl;
    }
  }
  vivaldi_coordinates_file.close();

  VivaldiNode own_node(my_vivaldi_coordinates_);

  uint64_t vivaldiIterations = 0;

  std::list<KnownOSD> known_osds;
  bool valid_known_osds = false;

  std::vector<uint64_t> currentRetries;
  int retriesInARow = 0;
  std::list<KnownOSD>::iterator chosen_osd_service;
  ZipfGenerator rankGen(options_.vivaldi_zipf_generator_skew);

  for (;;) {
    try {
      // Get a list of OSDs from the DIR(s)
      if ((vivaldiIterations % options_.vivaldi_max_iterations_before_updating)
           == 0) {
        valid_known_osds = UpdateKnownOSDs(&known_osds, own_node);
        if (valid_known_osds && !known_osds.empty()) {
          rankGen.set_size(known_osds.size());
        }
        // The pending retries are discarded, because the old OSDs might not
        // be in the new list
        currentRetries.clear();

        retriesInARow = 0;
        chosen_osd_service = known_osds.begin();
      }

      if (valid_known_osds && !known_osds.empty()) {
        if (retriesInARow == 0) {
          // Choose an OSD, only if there's no pending retry
          int ind = rankGen.next();

          std::list<KnownOSD>::iterator known_iterator = known_osds.begin();
          for (int i = 0;
              (i < ind) && (known_iterator != known_osds.end());
              known_iterator++, i++) {
            // Move the iterator over the chosen service
          }
          chosen_osd_service = known_iterator;
        }

        addressMappingGetRequest addr_request;
        addr_request.set_uuid(chosen_osd_service->GetUUID());

        boost::scoped_ptr< SyncCallback<AddressMappingSet> > response(
            ExecuteSyncRequest< SyncCallback<AddressMappingSet>* >(
                boost::bind(
                    &xtreemfs::pbrpc::DIRServiceClient
                        ::xtreemfs_address_mappings_get_sync,
                    dir_client_,
                    _1,
                    boost::cref(auth_bogus_),
                    boost::cref(user_credentials_bogus_),
                    &addr_request),
                dir_service_addresses_,
                NULL,
                options_.max_tries,
                options_,
                true,
                false));

        AddressMappingSet* addr_set = response->response();

        // Several mappings for the same UUID
        for (int i = 0; i < addr_set->mappings_size(); ++i) {
          const AddressMapping& mapping = addr_set->mappings(i);

          if (mapping.protocol() != xtreemfs::PBRPCURL::SCHEME_PBRPCU) {
            OSDServiceClient osd_client(rpc_client_);
            xtreemfs_pingMesssage ping_message;
            ping_message.set_request_response(true);
            ping_message.mutable_coordinates()
                ->MergeFrom(*own_node.GetCoordinates());

            VivaldiCoordinates* random_osd_vivaldi_coordinates;

            if (Logging::log->loggingActive(LEVEL_DEBUG)) {
              Logging::log->getLog(LEVEL_DEBUG)
                  << "Vivaldi: recalculating against: "
                  << chosen_osd_service->GetUUID() << std::endl;
            }

            UUIDIterator osd_service_address;
            osd_service_address.AddUUID(mapping.address() + ":"
                + boost::lexical_cast<std::string>(mapping.port()));

            // start timing
            boost::posix_time::ptime start_time(boost::posix_time
                ::microsec_clock::local_time());
            // execute sync ping
            boost::scoped_ptr< SyncCallback<xtreemfs_pingMesssage> >
                response_ping(
                    ExecuteSyncRequest< SyncCallback<xtreemfs_pingMesssage>* >(
                        boost::bind(
                            &xtreemfs::pbrpc::OSDServiceClient
                                ::xtreemfs_ping_sync,
                            osd_client,
                            _1,
                            boost::cref(auth_bogus_),
                            boost::cref(user_credentials_bogus_),
                            &ping_message),
                        &osd_service_address,
                        uuid_resolver_,
                        options_.max_tries,
                        options_,
                        true,
                        false));
            // stop timing
            boost::posix_time::ptime end_time(
                boost::posix_time::microsec_clock::local_time());
            boost::posix_time::time_duration rtt = end_time - start_time;
            uint64_t measuredRTT = rtt.total_milliseconds();

            // TODO(mno): check for request timeout, and start loop
            //            by continue if so

            random_osd_vivaldi_coordinates = response_ping->response()
                ->mutable_coordinates();

            if (Logging::log->loggingActive(LEVEL_DEBUG)) {
              Logging::log->getLog(LEVEL_DEBUG)
                  << "Vivaldi: ping response received." << std::endl;
            }

            // Recalculate coordinates here
            if (retriesInARow < options_.vivaldi_max_request_retries) {
              if (!own_node.RecalculatePosition(*random_osd_vivaldi_coordinates,
                                                measuredRTT,
                                                false)) {
                // The movement has been postponed because the measured RTT
                // seems to be a peak
                currentRetries.push_back(measuredRTT);
                retriesInARow++;
              } else {
                // The movement has been accepted
                currentRetries.clear();
                retriesInARow = 0;


                // Update client coordinates at the DIR
                if(options_.vivaldi_enable_dir_updates) {
                  if (Logging::log->loggingActive(LEVEL_DEBUG)) {
                    Logging::log->getLog(LEVEL_DEBUG)
                        << "Vivaldi: Sending coords to DIR." << std::endl;
                  }
                  boost::scoped_ptr< SyncCallback<emptyResponse> > response(
                      ExecuteSyncRequest< SyncCallback<emptyResponse>* >(
                          boost::bind(
                              &xtreemfs::pbrpc::DIRServiceClient
                                  ::xtreemfs_vivaldi_client_update_sync,
                              dir_client_,
                              _1,
                              boost::cref(auth_bogus_),
                              boost::cref(user_credentials_bogus_),
                              own_node.GetCoordinates()),
                          dir_service_addresses_,
                          NULL,
                          options_.max_tries,
                          options_,
                          true,
                          false));
                  response->DeleteBuffers();
                }
              }
            } else {
              // Choose the lowest RTT
              uint64_t lowestRTT = measuredRTT;
              for (std::vector<uint64_t>::iterator retries_iterator =
                  currentRetries.begin();
                  retries_iterator < currentRetries.end();
                  retries_iterator++) {
                if (*retries_iterator < lowestRTT) {
                  lowestRTT = *retries_iterator;
                }
              }  // for

              // Force recalculation after too many retries
              own_node.RecalculatePosition(*random_osd_vivaldi_coordinates,
                                           lowestRTT,
                                           true);
              currentRetries.clear();
              retriesInARow = 0;

              // set measuredRTT to the actually used one for trace output
              measuredRTT = lowestRTT;
            }

            // update local coordinate copy here
            {
              boost::mutex::scoped_lock lock(coordinate_mutex_);
              my_vivaldi_coordinates_.CopyFrom(*own_node.GetCoordinates());
            }
//            //Print a trace
//            char auxStr[256];
//            SPRINTF_VIV(auxStr,
//                        256,
//                        "%s:%lld(Viv:%.3f) Own:(%.3f,%.3f) lE=%.3f "
//                            "Rem:(%.3f,%.3f) rE=%.3f %s\n",
//                        retried ? "RETRY" : "RTT",
//                        static_cast<long long int> (measuredRTT),
//                        own_node.calculateDistance(
//                            (*own_node.getCoordinates()),
//                            random_osd_vivaldi_coordinates.get()),
//                        own_node.getCoordinates()->x_coordinate(),
//                        own_node.getCoordinates()->y_coordinate(),
//                        own_node.getCoordinates()->local_error(),
//                        random_osd_vivaldi_coordinates->x_coordinate(),
//                        random_osd_vivaldi_coordinates->y_coordinate(),
//                        random_osd_vivaldi_coordinates->local_error(),
//                        chosen_osd_service->get_uuid().data());
//            get_log()->getStream(YIELD::platform::Log::LOG_INFO) <<
//                "xtfs_vivaldi:" << auxStr;
//


            // Update OSD's coordinates
            chosen_osd_service->SetCoordinates(
                *random_osd_vivaldi_coordinates);

            // Re-sort known_osds
            std::list<KnownOSD> aux_osd_list(known_osds);
            KnownOSD chosen_osd_service_value = *chosen_osd_service;
            known_osds.clear();  // NOTE: this invalidates all ptrs and itrs

            for (std::list<KnownOSD>::reverse_iterator aux_iterator
                = aux_osd_list.rbegin();
                aux_iterator != aux_osd_list.rend();
                aux_iterator++) {
              double new_osd_distance =
                  own_node.CalculateDistance(
                      *(aux_iterator->GetCoordinates()),
                      *own_node.GetCoordinates());

              std::list<KnownOSD>::iterator known_iterator = known_osds.begin();

              while (known_iterator != known_osds.end()) {
                double old_osd_distance =  \
                      own_node.CalculateDistance(
                          *(known_iterator->GetCoordinates()),
                          *own_node.GetCoordinates());
                if (old_osd_distance >= new_osd_distance) {
                  known_osds.insert(known_iterator, (*aux_iterator));
                  break;
                } else {
                  known_iterator++;
                }
              }

              if (known_iterator == known_osds.end()) {
                known_osds.push_back((*aux_iterator));
              }
            }  // end re-sorting

            // find the chosen OSD in the resorted list
            chosen_osd_service = find(known_osds.begin(),
                                      known_osds.end(),
                                      chosen_osd_service_value);
            assert(chosen_osd_service != known_osds.end());
            response_ping->DeleteBuffers();
          }
        }
        response->DeleteBuffers();
      } else {
          if (Logging::log->loggingActive(LEVEL_WARN)) {
            Logging::log->getLog(LEVEL_WARN)
                << "Vivaldi: no OSD available." << std::endl;
          }
      }
    } catch (const std::exception& exc) {
        Logging::log->getLog(LEVEL_ERROR)
            << "Vivaldi: could not ping OSDs: " << exc.what() << std::endl;

      // TOFIX:This must be done only for timeout exceptions

      // We must avoid to keep retrying indefinitely against an OSD which is not
      // responding
      if (retriesInARow
          && (++retriesInARow >= options_.vivaldi_max_request_retries)) {
        // If the last retry times out all the previous retries are discarded
        currentRetries.clear();
        retriesInARow = 0;
      }
    }

    // Store the new coordinates in a local file
    if (Logging::log->loggingActive(LEVEL_DEBUG)) {
      Logging::log->getLog(LEVEL_DEBUG)
          << "Vivaldi: storing coordinates to file: ("
          << own_node.GetCoordinates()->x_coordinate() << ", "
          << own_node.GetCoordinates()->y_coordinate() << ")" << std::endl;
    }
    std::ofstream file_out(options_.vivaldi_filename.c_str(),
        std::ios_base::binary | std::ios_base::trunc);
    own_node.GetCoordinates()->SerializePartialToOstream(&file_out);
    file_out.close();

    // Sleep until the next iteration
    uint64_t sleep_in_s = static_cast<uint64_t>(
        options_.vivaldi_recalculation_interval_s -
        options_.vivaldi_recalculation_epsilon_s +
        (static_cast<double>(std::rand()) / (RAND_MAX - 1)) *
        2.0 * options_.vivaldi_recalculation_epsilon_s);

    if (Logging::log->loggingActive(LEVEL_DEBUG)) {
      Logging::log->getLog(LEVEL_DEBUG)
          << "Vivaldi: sleeping during " << sleep_in_s << " s." << std::endl;
    }
    try {
        boost::this_thread::sleep(boost::posix_time::seconds(sleep_in_s));
    } catch(const boost::thread_interrupted& e) {
        break;
    }

    vivaldiIterations = (vivaldiIterations + 1) % LONG_MAX;
  }
}  // Run()


bool Vivaldi::UpdateKnownOSDs(std::list<KnownOSD>* updated_osds,
                                const VivaldiNode& own_node) {
  bool retval = true;

  try {
    serviceGetByTypeRequest request;
    request.set_type(SERVICE_TYPE_OSD);

    boost::scoped_ptr< SyncCallback<ServiceSet> > response(
        ExecuteSyncRequest< SyncCallback<ServiceSet>* >(
            boost::bind(
                &xtreemfs::pbrpc::DIRServiceClient
                    ::xtreemfs_service_get_by_type_sync,
                dir_client_,
                _1,
                boost::cref(auth_bogus_),
                boost::cref(user_credentials_bogus_),
                &request),
            dir_service_addresses_,
            NULL,
            options_.max_tries,
            options_,
            true,
            false));

      ServiceSet* received_osds = response->response();
      updated_osds->clear();

      // Fill the list, ignoring every offline OSD
      for (int i = 0; i < received_osds->services_size(); ++i) {
        const Service& service = received_osds->services(i);
        if (service.last_updated_s() > 0) {  // only online OSDs
          const ServiceDataMap& sdm = service.data();
          const std::string* coordinates_string = NULL;
          for (int j = 0; j < sdm.data_size(); ++j) {
            if (sdm.data(j).key() == "vivaldi_coordinates") {
              coordinates_string = &sdm.data(j).value();
              break;
            }
          }

          // If the DS does not have the OSD's coordinates, we discard this
          // entry
          if (coordinates_string) {
            // Parse the coordinates provided by the DS
            VivaldiCoordinates osd_coords;
            OutputUtils::StringToCoordinates(*coordinates_string, osd_coords);
            KnownOSD new_osd(service.uuid(), osd_coords);

            // Calculate the current distance from the client to the new OSD
            double new_osd_distance = own_node.CalculateDistance(
                 *(own_node.GetCoordinates()),
                 osd_coords);

            std::list<KnownOSD>::iterator up_iterator = updated_osds->begin();
            while (up_iterator != updated_osds->end()) {
              double old_osd_distance =
                  own_node.CalculateDistance(*up_iterator->GetCoordinates(),
                                             *(own_node.GetCoordinates()));
              if (old_osd_distance >= new_osd_distance) {
                updated_osds->insert(up_iterator, new_osd);
                break;
              } else {
                up_iterator++;
              }
            }

            if (up_iterator == updated_osds->end()) {
              updated_osds->push_back(new_osd);
            }
          }  // if (coordinates_string)
        }
      }  // for
    response->DeleteBuffers();
  } catch (const std::exception& ex) {
    Logging::log->getLog(LEVEL_ERROR)
       << "Vivaldi: Failed to update known OSDs:" << ex.what() << std::endl;
    retval = false;
  }

  return retval;
}  // update_known_osds

const VivaldiCoordinates& Vivaldi::GetVivaldiCoordinates() const {
  boost::mutex::scoped_lock lock(coordinate_mutex_);
  return my_vivaldi_coordinates_;
}

}  // namespace xtreemfs

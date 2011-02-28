/* Serialization of mongoDeploy types */

#ifndef MONGODEPLOY_SERIALIZE_H_
#define MONGODEPLOY_SERIALIZE_H_

#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

namespace boost {
namespace serialization {

template <class Archive> void serialize (Archive & ar, mongoDeploy::ReplicaSet & x, const unsigned version) {
	ar & x.replicas;
}

template <class Archive> void serialize (Archive & ar, mongoDeploy::ConfigSet & x, const unsigned version) {
	ar & x.cfgServers;
}

template <class Archive> void serialize (Archive & ar, mongoDeploy::ShardSet & x, const unsigned version) {
	ar & x.configSet;
	ar & x.routers;
	ar & x.shards;
}

}}

#endif /* MONGODEPLOY_SERIALIZE_H_ */

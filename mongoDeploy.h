/* Deploy mongo processes on servers */

#pragma once

#include <utility>
#include <vector>
#include <mongo/client/dbclient.h>
#include <10remote/remote.h>
#include <10remote/process.h>
#include <cassert>

namespace mongoDeploy {

/** Connection **/

/** Host and port of a mongoD/S process */
std::string hostPortString (remote::Process mongoProcess);
mongo::HostAndPort hostAndPort (remote::Process mongoProcess);

typedef boost::shared_ptr<mongo::DBClientConnection> Connection;

/** Try to connect every 2 secs until successful. Give up after maxSecs (60 secs by default) */
Connection waitConnect (std::string hostPort, unsigned maxSecs = 60);
Connection waitConnect (remote::Process mongoProcess, unsigned maxSecs = 60);

/** MongoD **/

/** Prefix for data directory, a number get appended to this, eg. "dbms" + "1" */
extern std::string mongoDbPathPrefix;
/** Default MongoD config is merged with user supplied config. User config options take precedence */
extern program::Options defaultMongoD;

typedef remote::Process MongoD;

/** Start mongod program with given options +
 * unique values generated for dbpath and port options if not already supplied +
 * defaultMongoD options where not already supplied. */
MongoD startMongoD (remote::Host, program::Options = program::Options());

//inline bool isMongoD (remote::Process p) {return p.process.program.executable == "mongod";}

/** Replica set **/

/** Server command-line options + member replSetConfig options for a single replica in a replica set */
struct RsMemberSpec {
	program::Options opts;
	mongo::BSONObj memberConfig;
	RsMemberSpec (program::Options opts, mongo::BSONObj memberConfig) : opts(opts), memberConfig(memberConfig) {}
	RsMemberSpec () {} // for serialization
};

/** Replica set of mongoD processes. RS name and config can be gotten from any replica process */
class ReplicaSet {
public:
	std::vector<MongoD> replicas;
	std::vector<RsMemberSpec> memberSpecs;
	ReplicaSet (std::vector<MongoD> replicas, std::vector<RsMemberSpec> memberSpecs) : replicas(replicas), memberSpecs(memberSpecs)
		{assert (replicas.size() == memberSpecs.size());}
	ReplicaSet () {}  // for serialization
	std::string name();  // replica set name gotten from first replica's 'replSet' option.
	/** Active replicas. Excludes arbiter and passive replicas */
	std::vector<MongoD> activeReplicas();
	/** Replica-set name "/" comma-separated hostPorts of active members only */
	std::string nameActiveHosts();
	/** Start mongod and add it to replica set */
	void addStartReplica (remote::Host, RsMemberSpec);
	/** Remove i'th replica and stop it */
	void removeStopReplica (unsigned i);
};

/** Start replica set on given servers with given config options + generated 'replSet' and options filled in by 'startMongoD' (if not already supplied). Set-wide rsSettings can also be supplied. See http://www.mongodb.org/display/DOCS/Replica+Set+Configuration for config details. */
ReplicaSet startReplicaSet (std::vector<remote::Host>, std::vector<RsMemberSpec>, mongo::BSONObj rsSettings = mongo::BSONObj());

/** Shard cluster **/

/** Sharding config servers */
class ConfigSet {
public:
	std::vector<MongoD> cfgServers;
	ConfigSet (std::vector<MongoD> cfgServers) : cfgServers(cfgServers) {}
	ConfigSet () {}  // for serialization
};

/** Start config mongoD on each host. 1 or 3 hosts expected */
ConfigSet startConfigSet (std::vector<remote::Host>, program::Options = program::Options());

/** Default MongoS config is merged with user supplied config. User config options take precedence */
extern program::Options defaultMongoS;

typedef remote::Process MongoS;

/** Start mongos program with given options +
 * unique values generated for dbpath and port options if not already supplied +
 * defaultMongoS options where not already supplied. */
MongoS startMongoS (remote::Host, ConfigSet, program::Options = program::Options());

/** A full sharding deployment with routers (mongos), config servers (ConfigSet), and ReplicaSet shards */
class ShardSet {
public:
	ConfigSet configSet;
	std::vector<MongoS> routers;
	std::vector<ReplicaSet> shards;
	/** ShardSet starts out empty (no shards) */
	ShardSet (ConfigSet configSet, std::vector<MongoS> routers) : configSet(configSet), routers(routers) {}
	ShardSet () {}  // for serialization
	/** Start replica set of given specs on given hosts, and add it as another shard */
	void addStartShard (std::vector<remote::Host>, std::vector<RsMemberSpec>, mongo::BSONObj rsSettings = mongo::BSONObj());
	/** Remove i'th shard and stop it */
	void removeStopShard (unsigned i);
	/** Start mongos and add it to available routers */
	void addStartRouter (remote::Host, program::Options = program::Options());
	/** Remove i'th router from list and stop it */
	void removeStopRouter (unsigned i);
};

/** Start empty shard set with given config server specs and router (mongos) specs */
ShardSet startShardSet (std::vector<remote::Host> cfgHosts, std::vector<remote::Host> routerHosts, program::Options cfgOpts = program::Options(), program::Options routerOpts = program::Options());

/** Enable sharding on given database */
void shardDatabase (std::string mongoSHostPort, std::string database);
inline void shardDatabase (MongoS mongoS, std::string database) {shardDatabase (hostPortString (mongoS), database);}

/** Shard collection on key. FullCollection includes database prefix. */
void shardCollection (std::string mongoSHostPort, std::string fullCollection, mongo::BSONObj shardKey);
inline void shardCollection (MongoS mongoS, std::string fullCollection, mongo::BSONObj shardKey) {shardCollection (hostPortString (mongoS), fullCollection, shardKey);}

}

/* Printing */

// #include <10util/util.h> // output vector

inline std::ostream& operator<< (std::ostream& out, const mongoDeploy::RsMemberSpec& x) {
	out << "RsMemberSpec " << program::optionsString (x.opts) << " " << x.memberConfig;
	return out;}

inline std::ostream& operator<< (std::ostream& out, const mongoDeploy::ReplicaSet& x) {
	out << "ReplicaSet " << x.replicas << " " << x.memberSpecs;
	return out;}

inline std::ostream& operator<< (std::ostream& out, const mongoDeploy::ConfigSet& x) {
	out << "ConfigSet " << x.cfgServers;
	return out;}

inline std::ostream& operator<< (std::ostream& out, const mongoDeploy::ShardSet& x) {
	out << "ShardSet " << x.configSet << " " << x.routers << " " << x.shards;
	return out;}

/* Serialization */

#include <boost/serialization/split_free.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

BOOST_SERIALIZATION_SPLIT_FREE (mongo::BSONObj)

namespace boost {
namespace serialization {

template <class Archive> void save (Archive& ar, const mongo::BSONObj& x, const unsigned version) {
	unsigned n = x.objsize();
	ar << n;
	ar.save_binary (x.objdata(), n);
}

template <class Archive> void load (Archive& ar, mongo::BSONObj& x, const unsigned version) {
	unsigned n;
	ar >> n;
	char* data = new char[n];
	ar.load_binary (data, n);
	x = mongo::BSONObj (data) .copy(); //TODO: create owned object directly on data (and don't delete data)
	delete[] data;
}

template <class Archive> void serialize (Archive & ar, mongoDeploy::RsMemberSpec & x, const unsigned version) {
	ar & x.opts;
	ar & x.memberConfig;
}

template <class Archive> void serialize (Archive & ar, mongoDeploy::ReplicaSet & x, const unsigned version) {
	ar & x.replicas;
	ar & x.memberSpecs;
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

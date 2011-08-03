/* */

#include "mongoDeploy.h"
#include <10util/util.h>
#include <boost/algorithm/string.hpp>
#include <10util/thread.h>
#include <10util/except.h>

using namespace std;

/** Connection **/

static unsigned DefaultPort = 27017;

/** Host and port of a mongoD/S process */
string mongoDeploy::hostPortString (remote::Process mongoProcess) {
	boost::optional<string> port = program::lookup ("port", remote::program(mongoProcess).options);
	return remote::hostPort(mongoProcess.host).hostname + ":" + (port ? *port : to_string(DefaultPort));
}

mongo::HostAndPort mongoDeploy::hostAndPort (remote::Process mongoProcess) {
	return mongo::HostAndPort (hostPortString (mongoProcess));
}

/** Try to connect every 2 secs until successful. Give up after maxSecs (60 secs by default) */
mongoDeploy::Connection mongoDeploy::waitConnect (string hostPort, unsigned maxSecs) {
	unsigned secs = 0;
	while (true)
		try {
			Connection c (new mongo::DBClientConnection);
			c->connect (hostPort);
			return c;
		} catch (exception &e) {
			if (secs >= maxSecs) except::raise (e);
			thread::sleep (2);
			secs += 2;
		}
}

/** Try to connect every 2 secs until successful. Give up after maxSecs (60 secs by default) */
mongoDeploy::Connection mongoDeploy::waitConnect (remote::Process mongoProcess, unsigned maxSecs) {
	return waitConnect (hostPortString (mongoProcess), maxSecs);}

/** MongoD **/

/** Prefix for data directory, a number get appended to this, eg. "dbms" + "1" */
string mongoDeploy::mongoDbPathPrefix = "dbms";  // in current directory
/** Default MongoD config is merged with user supplied config. User config options take precedence */
program::Options mongoDeploy::defaultMongoD;

static volatile unsigned long nextDbPath;

/** start mongod program with given options +
 * unique values generated for dbpath and port options if not already supplied +.
 * defaultMongoD options where not already supplied. */
mongoDeploy::MongoD mongoDeploy::startMongoD (remote::Host host, program::Options options) {
	program::Options config;
	config.push_back (make_pair (string ("rest"), ""));
	config.push_back (make_pair (string ("dbpath"), mongoDbPathPrefix + to_string (++ nextDbPath)));
	config.push_back (make_pair (string ("port"), to_string (27100 + nextDbPath)));
	program::Options config1 = program::merge (config, defaultMongoD);
	program::Options config2 = program::merge (config1, options);  //user options have precedence
	string path = * program::lookup ("dbpath", config2);
	stringstream ss;
	ss << "rm -rf " << path << " && mkdir -p " << path;
	program::Program program (ss.str(), "mongod", config2);
	return remote::launch (program, host);
}

/** Replica set */

/** Good if one primary and rest secondaries and arbiters */
static bool goodReplStatus (mongo::BSONObj &info) {
	if (!info.getField("ok").trueValue()) return false;
	vector<mongo::BSONElement> ms = info.getField("members").Array();
	bool primary = false;
	for (unsigned i = 0; i < ms.size(); i++) {
		int state = ms[i].Obj().getIntField("state");
		if (state == 1) {primary = true; continue;}
		if (state != 2 && state != 7) return false;
	}
	return primary;
}

static mongo::BSONObj waitForGoodReplStatus (mongoDeploy::Connection c, unsigned maxSecs = 60) {
	unsigned secs = 0;
	mongo::BSONObj info;
	while (true) {
		if (secs >= maxSecs) throw runtime_error ("replica set failed to initiate: " + to_string (info));
		c->runCommand ("admin", BSON ("replSetGetStatus" << 1), info);
		if (goodReplStatus (info)) return info;
		thread::sleep (2);
		secs += 2;
	}
}

static volatile unsigned long nextReplicaSetId;

/** Start replica set with given member specs and config options + generated 'replSet' and options filled in by 'startMongoD' (if not already supplied) */
mongoDeploy::ReplicaSet mongoDeploy::startReplicaSet (vector<remote::Host> hosts, vector<RsMemberSpec> memberSpecs, mongo::BSONObj rsSettings) {
	assert (hosts.size() == memberSpecs.size());
	if (memberSpecs.size() == 0) throw runtime_error ("can't create empty replica set");
	program::Options options;
	string rsName = "rs" + to_string (++ nextReplicaSetId);
	options.push_back (make_pair ("replSet", rsName));
	vector<MongoD> replicas;
	mongo::BSONArrayBuilder members;
	for (unsigned i = 0; i < min (hosts.size(), memberSpecs.size()); i++) {
		MongoD p = startMongoD (hosts[i], program::merge (options, memberSpecs[i].opts));
		replicas.push_back (p);
		mongo::BSONObjBuilder obj;
		obj.appendElements (BSON ("_id" << i << "host" << hostPortString (p)));
		obj.appendElements (memberSpecs[i].memberConfig);
		members.append (obj.done());
	}
	Connection c = waitConnect (replicas[0]);
	for (unsigned i = 1; i < replicas.size(); i++) waitConnect (replicas[i]);
	mongo::BSONObj rsConfig = BSON ("_id" << rsName << "members" << members.arr() << "settings" << rsSettings);
    mongo::BSONObj info;
    cout << "replSetInitiate: " << rsConfig << " ->" << endl;
    c->runCommand ("admin", BSON ("replSetInitiate" << rsConfig), info);
    cout << " " << info << endl;
    cout << "replSetGetStatus -> " << flush;
    info = waitForGoodReplStatus (c);
    cout << info << endl;
	return ReplicaSet (replicas, memberSpecs);
}

/** Extract replica set name from replica set name + seed list */
static string parseReplSetName (string replSetString) {
	vector<string> parts;
	boost::split (parts, replSetString, boost::is_any_of ("/"));
	return *parts.begin();
}

string mongoDeploy::ReplicaSet::name () {
	string replSetString = * program::lookup ("replSet", remote::program (replicas[0]) .options);
	return parseReplSetName (replSetString);
}

/** Active (non-arbiter, non-passive) replicas in replica set */
// TODO: exclude passive too
vector <mongoDeploy::MongoD> mongoDeploy::ReplicaSet::activeReplicas () {
	assert (replicas.size() == memberSpecs.size());
	vector <MongoD> active;
	for (unsigned i = 0; i < replicas.size(); i++) {
		if (! memberSpecs[i].memberConfig.getBoolField ("arbiterOnly"))
			active.push_back (replicas[i]);
	}
	return active;
}

/** Replica-set name "/" comma-separated hostPorts of active hosts.
 * Passive hosts are not included because it breaks addshard command */
string mongoDeploy::ReplicaSet::nameActiveHosts () {
	return name() + "/" + concat (intersperse (string(","), fmap (hostPortString, activeReplicas())));
}

static void addReplica (mongoDeploy::ReplicaSet rs, remote::Process mongod, mongo::BSONObj memberConfig) {
	vector<mongo::HostAndPort> hs = fmap (mongoDeploy::hostAndPort, rs.replicas);
	mongo::DBClientReplicaSet c (rs.name(), hs);
	bool ok = c.connect();
	if (!ok) throw runtime_error ("Unable to connect to replica set " + rs.name());

	mongo::BSONObj cfg = c.findOne ("local.system.replset", mongo::BSONObj());
	if (cfg.isEmpty()) throw runtime_error ("Missing replica set config " + rs.name());
	int ver = cfg.getIntField ("version");
	cfg = cfg.replaceFieldNames (BSON ("version" << ver+1));
	throw "TODO: finish addReplica";
}

/** Start mongod and add it to replica set */
void mongoDeploy::ReplicaSet::addStartReplica (remote::Host host, RsMemberSpec memberSpec) {
	program::Options options;
	options.push_back (make_pair ("replSet", name()));
	MongoD proc = startMongoD (host, program::merge (options, memberSpec.opts));
	waitConnect (proc);
	addReplica (*this, proc, memberSpec.memberConfig);
}

/** Remove i'th replica and stop it */
void removeStopReplica (unsigned i) {
	throw "TODO removeStopReplica";
}

/** Shard cluster **/

/** Start config mongoD on each host. 1 or 3 hosts expected */
mongoDeploy::ConfigSet mongoDeploy::startConfigSet (vector<remote::Host> hosts, program::Options opts) {
	vector<MongoD> procs;
	for (unsigned i = 0; i < hosts.size(); i++) procs.push_back (startMongoD (hosts[i], opts));
	for (unsigned i = 0; i < procs.size(); i++) waitConnect (procs[i]);
	return ConfigSet (procs);
}

/** Default MongoD config is merged with user supplied config. User config options take precedence */
program::Options mongoDeploy::defaultMongoS;

/** Start mongos program with given options +
 * unique values generated for dbpath and port options if not already supplied +.
 * defaultMongoS where not already supplied. */
mongoDeploy::MongoS mongoDeploy::startMongoS (remote::Host host, ConfigSet cs, program::Options options) {
	program::Options config;
	config.push_back (make_pair (string ("port"), to_string (27100 + (++ nextDbPath))));
	config.push_back (make_pair (string ("configdb"), concat (intersperse (string(","), fmap (hostPortString, cs.cfgServers)))));
	program::Options config1 = program::merge (config, defaultMongoS);
	program::Options config2 = program::merge (config1, options);  //user options have precedence
	program::Program program;
	program.executable = "mongos";
	program.options = config2;
	return remote::launch (program, host);
}

/** Start empty shard set with given config server specs and router (mongos) specs */
mongoDeploy::ShardSet mongoDeploy::startShardSet (vector<remote::Host> cfgHosts, vector<remote::Host> routerHosts, program::Options cfgOpts, program::Options routerOpts) {
	ConfigSet cs = startConfigSet (cfgHosts, cfgOpts);
	boost::function1<MongoS,remote::Host> f = boost::bind (startMongoS, _1, cs, routerOpts);
	vector<MongoS> rs = fmap (f, routerHosts);
	for (unsigned i = 0; i < rs.size(); i++) waitConnect (rs[i]);
	return ShardSet (cs, rs);
}

static void addShard (mongoDeploy::ShardSet& s, mongoDeploy::ReplicaSet r) {
	mongo::DBClientConnection c;
	c.connect (mongoDeploy::hostPortString (s.routers[0]));
    mongo::BSONObj info;
    mongo::BSONObj cmd = BSON ("addshard" << r.nameActiveHosts());
    cout << cmd << " -> " << endl;
    c.runCommand ("admin", cmd, info);
    cout << " " << info << endl;
    c.runCommand ("admin", BSON ("listshards" << 1), info);
    cout << "listshards -> " << info << endl;
    s.shards.push_back (r);
}

/** Start replica set of given specs on given hosts, and add it as another shard */
void mongoDeploy::ShardSet::addStartShard (vector<remote::Host> hosts, vector<RsMemberSpec> memberSpec, mongo::BSONObj rsSettings) {
	ReplicaSet r = startReplicaSet (hosts, memberSpec, rsSettings);
	addShard (*this, r);
}

/** Remove i'th shard and stop it */
void mongoDeploy::ShardSet::removeStopShard (unsigned i) {
	throw "TODO removeStopShard";
}

/** Start mongos and add it to available routers */
void mongoDeploy::ShardSet::addStartRouter (remote::Host host, program::Options opts) {
	throw "TODO addStartRouter";
}

/** Remove i'th router from list and stop it */
void mongoDeploy::ShardSet::removeStopRouter (unsigned i) {
	throw "TODO remoteStopRouter";
}

/** Enable sharding on given database */
void mongoDeploy::shardDatabase (string mongoSHostPort, string database) {
	using namespace mongo;
	BSONObj info;
	DBClientConnection c;
	c.connect (mongoSHostPort);
	BSONObj cmd = BSON ("enablesharding" << database);
	cout << cmd << " -> " << endl;
	c.runCommand ("admin", cmd, info);
	cout << info << endl;
}

/** Shard collection on key */
void mongoDeploy::shardCollection (string mongoSHostPort, string fullCollection, mongo::BSONObj shardKey) {
	using namespace mongo;
	BSONObj info;
	DBClientConnection c;
	c.connect (mongoSHostPort);
	BSONObj cmd = BSON ("shardcollection" << fullCollection << "key" << shardKey);
	cout << cmd << " -> " << endl;
	c.runCommand ("admin", cmd, info);
	cout << info << endl;
}

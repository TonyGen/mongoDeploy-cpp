/* Assumes util, remote, and mongoDeploy libraries have been built and installed in /usr/local/include and /usr/local/lib.
 * Compile as: g++ shardSet.cpp -o shardSet -I/opt/local/include -L/opt/local/lib -lboost_system-mt -lboost_filesystem-mt -lboost_thread-mt -lboost_serialization-mt -l10util -lremote -lmongoDeploy -lmongoclient -lpcre
 * Run as: `shardSet` */

#include <mongoDeploy/mongoDeploy.h>
#include <10util/thread.h>

using namespace std;

static mongoDeploy::ReplicaSet startReplicaSet () {
	vector<remote::Host> hosts;
	hosts.push_back ("localhost");
	hosts.push_back ("localhost");
	hosts.push_back ("localhost");
	vector<mongoDeploy::RsMemberSpec> specs;
	specs.push_back (mongoDeploy::RsMemberSpec (program::Options(), mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (program::Options(), mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (program::Options(), BSON ("arbiterOnly" << true)));
	return mongoDeploy::startReplicaSet (hosts, specs);
}

void testReplicaSet () {
	mongoDeploy::ReplicaSet rs = startReplicaSet();
	cout << rs.nameActiveHosts() << endl;
}

/** Specification for a replica set of given number of active servers */
static vector<mongoDeploy::RsMemberSpec> rsSpecWithArbiter (unsigned numActiveServers) {
	std::vector<mongoDeploy::RsMemberSpec> specs;
	for (unsigned i = 0; i < numActiveServers; i++)
		specs.push_back (mongoDeploy::RsMemberSpec (program::options ("dur", "", "noprealloc", "", "oplogSize", "200"), mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (program::options ("dur", "", "noprealloc", "", "oplogSize", "4"), BSON ("arbiterOnly" << true)));
	return specs;
}

static mongoDeploy::ShardSet startShardSet() {
	// Launch empty shard set with one config server and one mongos with small chunk size
	vector<remote::Host> hosts;
	hosts.push_back ("localhost");
	mongoDeploy::ShardSet s = mongoDeploy::startShardSet (hosts, hosts, program::Options(), program::options ("chunkSize", "2"));
	// Launch two shards, each a replica set of 2 severs and one arbiter
	hosts.push_back ("localhost");
	hosts.push_back ("localhost");
	s.addStartShard (hosts, rsSpecWithArbiter(2));
	s.addStartShard (hosts, rsSpecWithArbiter(2));
	return s;
}

/** All replica-set shard processes excluding arbiters */
static vector<rprocess::Process> activeShardProcesses (mongoDeploy::ShardSet s) {
	vector<rprocess::Process> procs;
	for (unsigned i = 0; i < s.shards.size(); i ++) {
		vector<rprocess::Process> rsProcs = s.shards[i].activeReplicas();
		for (unsigned j = 0; j < rsProcs.size(); j ++) procs.push_back (rsProcs[j]);
	}
	return procs;
}

/** Kill random server every once in a while and restart it */
static Unit killer (mongoDeploy::ShardSet s) {
	vector<rprocess::Process> procs = activeShardProcesses (s);
	thread::sleep (rand() % 10);
	while (true) {
		unsigned r = rand() % procs.size();
		rprocess::Process p = procs[r];
		rprocess::signal (SIGKILL, p);
		cout << "Killed " << p << endl;
		thread::sleep (rand() % 30);
		rprocess::restart (p);
		cout << "Restarted  " << p << endl;
		thread::sleep (rand() % 60);
	}
	return unit;
}

void testShardSet () {
	mongoDeploy::ShardSet s = startShardSet();
	cout << s.shards.size() << endl;
	killer (s);
}

int main (int argc, const char* argv[]) {
	boost::shared_ptr<boost::thread> th = remote::listen();
	testShardSet();
	th->join();
}

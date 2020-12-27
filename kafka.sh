#!/bin/bash

kafka_home=/usr/local/lib/kafka_2.13-2.7.0

case $1 in
	start)
		echo "Zookeeper start"
		$kafka_home/bin/zookeeper-server-start.sh -daemon $kafka_home/config/zookeeper.properties
		echo "Kafka start"
		$kafka_home/bin/kafka-server-start.sh -daemon $kafka_home/config/server.properties
		;;	
	stop)
		echo "Kafka stop"
		$kafka_home/bin/kafka-server-stop.sh
		echo "Zookeeper stop"
		$kafka_home/bin/zookeeper-server-stop.sh
		;;
	*) echo "$0 {start|stop}"
		exit 4
		;;
esac

#!/usr/bin/env ruby

=begin

CREATE DATABASE blemesh
CREATE USER mqtt WITH PASSWORD mqtt
GRANT ALL ON blemesh TO mqtt

drop series from "temperature"

=end

begin
    require 'paho-mqtt'
    require 'influxdb'
rescue Exception => e
    STDERR.puts "Exception => #{e.class}: #{e.message}."
    exit 1
end

INFLUX = {
    :ip       => "127.0.0.1",
    :port     => "8086",
    :user     => "mqtt",
    :password => "mqtt",
    :database => "blemesh",
}

MQTT = {
    :ip => "127.0.0.1",
    :port => 1883,
    :topic => "/sensors/results/dashboard",
}

influxdb = InfluxDB::Client.new INFLUX[:database],
        host: INFLUX[:ip],
        port: INFLUX[:port],
        user: INFLUX[:user],
        password: INFLUX[:password]


mqtt = PahoMqtt::Client.new({:host => MQTT[:ip], :port => MQTT[:port], :ssl => false})


# Receive messages
mqtt.on_message do |message|
    response = JSON.parse(message.payload)
    puts response
    if response.key? "type"
        if response["type"].eql? "measure"
            data = {
                :values => { :value => response["value"]},
                :tags   => { :addr  => response["addr"]}
            }
            influxdb.write_point('temperature', data)
        end
    end
end

### Register a callback on suback to assert the subcription
waiting_suback = true
mqtt.on_suback do
    waiting_suback = false
end

### Connect to the eclipse test server on port 1883 (Unencrypted mode)
mqtt.connect

### Subscribe to a topic
mqtt.subscribe(MQTT[:topic], 1)

### Waiting for the suback answer and excute the previously set on_suback callback
while waiting_suback do
    sleep 0.001
end


loop {}
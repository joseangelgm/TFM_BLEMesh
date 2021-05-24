#!/usr/bin/env ruby

require 'paho-mqtt'
require 'json'

actions = {
    'actions' => [
        {'task_name' => 'task_get_status'}
    ]
}

=begin
actions = {
    'actions' => [
        'task' => {
            'auto' => true,
            'opcode' => 'GET_STATUS',
            'delay' => 5,
            'name' => 'task_get_status'
        }
    ]
}
=end

pp actions
puts JSON[actions]

### Create a simple client with default attributes
client = PahoMqtt::Client.new

### Connect to the eclipse test server on port 1883 (Unencrypted mode)
client.connect('192.168.0.183', 1883)

### Publlish a message on the topic "/paho/ruby/test" with "retain == false" and "qos == 1"
client.publish("/sensors/commands", JSON[actions], false, 1)

### Waiting to assert that the message is displayed by on_message callback
sleep 1

### Calling an explicit disconnect
client.disconnect
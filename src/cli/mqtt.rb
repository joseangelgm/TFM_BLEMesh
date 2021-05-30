#!/usr/bin/env ruby

require 'optparse'
require 'json'
require 'pathname'

begin
    require 'paho-mqtt'
rescue
    STDERR.puts "#{paho-mqtt} is required. Please install it."
    exit 1
end


def send_mqtt_json(actions)

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
end

options = {}
optparser = OptionParser.new do |opts|

    opts.banner = "Usage: #{File.basename(__FILE__)} [-c] [-r]"

    options[:create] = false
    opts.on('-c', '--create', 'Create a new task') do
        options[:create] = true
    end

    options[:remove] = false
    opts.on('-r', '--remove', 'Remove an existing task') do
        options[:remove] = true
    end

    opts.on( '-h', '--help', 'Display this screen' ) do
        puts opts
        exit 0
    end
end

optparser.parse!

=begin
actions = {
    'actions' => [
        {'task_name' => 'task_get_status'}
    ]
}


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
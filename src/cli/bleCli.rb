#!/usr/bin/env ruby

=begin
{
    "actions" : [
        {
            "auto"   : true,
            "addr"   : "00aa",
            "opcode" : "GET_STATUS",
            "delay"  : 5,
            "name"   : "task_get_status"
        },
        {
            "auto"   : true,
            "addr"   : "00bb",
            "opcode" : "GET_STATUS",
            "delay"  : 5,
            "name"   : "task_get"
        }
    ]
}

{
    "actions" : [
        {
            "auto"   : true,
            "addr"   : "00aa",
            "opcode" : "GET_STATUS",
            "delay"  : 5,
            "name"   : "task_get_status_2"
        }
    ]
}

{
    "bad" : true
}
=end

$LOAD_PATH << __dir__ + '/classes/'

require 'optparse'
require 'actionParser'
require 'mqtt'
require 'yaml'

require 'pry-byebug' # Delete. Only for testing purposes

begin
    require 'paho-mqtt'
rescue
    STDERR.puts "paho-mqtt is required. Please install it."
    exit 1
end

CONFIG_FILE = "./config/config.yaml"

def format_param_structure(params)

    array_params = []

    case params
    when Hash;
        params.each do |k, v|
            array_params << v
        end
    when Array;
        array_params = params
    else
        puts "Parameters could be recognize to format"
        exit 1
    end
    array_params
end

def command(optparser, param, &block)
    optparser.on(*format_param_structure(param), block)
end

ACTIONS = {
    :short => "-a",
    :large => "--actions",
    :help  => "Create or remove tasks",
}

TASKS = {
    :short => "-t",
    :large => "--tasks",
    :help  => "Obtain a json with tasks info",
}

EDITOR = {
    :short => "-e",
    :large => "--editor [editor]",
    :help  => "Select the text editor. By default is taken from EDITOR environment variable",
    :type  => String
}

HELP = {
    :short => "-h",
    :large => "--help",
    :help  => "Display this help",
}

CMD = {
    :tasks => {'cmd' => 'tasks'},
}

options = {}
optparser = OptionParser.new

command optparser, ACTIONS do
    options[:actions] = true
end

command optparser, TASKS do
    options[:tasks] = true
end

command optparser, EDITOR do |elems|
    if elems.nil?
        puts "You have to provide a text editor!"
        puts optparser
        exit 1
    end
    options[:editor] = elems.chomp
end

command optparser, HELP do
    puts optparser
    exit 0
end

optparser.parse ARGV

if ARGV.length == 0
    puts optparser
    exit 1
end

editor = options[:editor] || ENV['EDITOR'] # parameter or environment variable
if !editor && options[:actions]
    puts "Use -e parameter or create EDITOR environment variable\n"
    exit 1
end

config = YAML::load_file(CONFIG_FILE)



if options[:actions]
    begin
        # create mqtt object
        mqtt_client = MQTT.new(config[:mqtt_ble])

        # Build json
        actions = ActionParser.new
        actions.create_json(editor)
        puts

        # Send json and prompt response
        mqtt_client.send_json(actions.actions_json)
        json_received = mqtt_client.response
        puts "#{"Json received".bold.green}"
        pp json_received
    rescue PahoMqtt::Exception => e
        STDERR.puts "#{"Exception".bold.red} => #{e.class}: #{e.message}\n"\
                    "Ensule that mqtt is powered on."
        exit 1
    rescue Timeout::Error => e
        STDERR.puts "#{"Exception".bold.red} => #{e.class}: #{e.message}\n"\
                    "The client spent more time than #{mqtt_client.time_to_wait} seconds. Set more timeout in #{CONFIG_FILE}"
        exit 1
    rescue Exception => e
        STDERR.puts "#{"Exception".bold.red} => #{e.class}: #{e.message}\n#{e.backtrace.join("\n")}."
        exit 1
    end
end

if options[:tasks]
    begin
        # create mqtt object
        mqtt_client = MQTT.new(config[:mqtt_cmd])

        # Send json and prompt response
        mqtt_client.send_json(CMD[:tasks])
        json_received = mqtt_client.response
        puts "#{"Json received".bold.green}"
        pp json_received
    rescue PahoMqtt::Exception => e
        STDERR.puts "#{"Exception".bold.red} => #{e.class}: #{e.message}\n"\
                    "Ensule that mqtt is powered on."
        exit 1
    rescue Timeout::Error => e
        STDERR.puts "#{"Exception".bold.red} => #{e.class}: #{e.message}\n"\
                    "The client spent more time than #{mqtt_client.time_to_wait} seconds. Set more timeout in #{CONFIG_FILE}"
        exit 1
    rescue Exception => e
        STDERR.puts "#{"Exception".bold.red} => #{e.class}: #{e.message}\n#{e.backtrace.join("\n")}."
        exit 1
    end
end
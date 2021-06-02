#!/usr/bin/env ruby

=begin
actions = {
    'actions' => [
        {'task_name' => 'task_get_status'}
    ]
}


actions = {
    'actions' => [
        {
            'auto' => true,
            'opcode' => 'GET_STATUS',
            'delay' => 5,
            'name' => 'task_get_status'
        },
        {
            'auto' => true,
            'opcode' => 'GET_STATUS',
            'delay' => 5,
            'name' => 'task_get'
        }
    ]
}

{
    "actions" : [
        {
            "auto"   : true,
            "opcode" : "GET_STATUS",
            "delay"  : 5,
            "name"   : "task_get_status"
        },
        {
            "auto"   : true,
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
            "opcode" : "GET_STATUS",
            "delay"  : 5,
            "name"   : "task_get_status"
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

CONFIG_FILE = "./config.yaml"

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

CREATE = {
    :short => "-c",
    :large => "--create",
    :help  => "Create a new task",
}

REMOVE = {
    :short => "-r",
    :large => "--remove",
    :help  => "Remove a task with a given name",
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

options = {}
optparser = OptionParser.new

command optparser, CREATE do
    options[:create] = true
end

command optparser, REMOVE do
    options[:remove] = true
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

config = YAML::load_file(CONFIG_FILE)
mqtt_client   = nil

begin
    mqtt_client = MQTT.new(config[:mqtt])
    mqtt_client.send_json({:pepe => "pepe"})
    json_received = mqtt_client.response
    puts "#{"Json received".bold.green}"
    pp json_received
rescue PahoMqtt::Exception => e
    STDERR.puts "#{"Exception".bold.red} => #{e.class}: #{e.message}\n"\
                "Ensule that mqtt is powered on."
    exit 1
rescue Timeout::Error => e
    STDERR.puts "#{"Exception".bold.red} => #{e.class}: #{e.message}\n"\
                "The client spend more time than expected. Set timeout in #{CONFIG_FILE}"
    exit 1
rescue Exception => e
    STDERR.puts "#{"Exception".bold.red} => #{e.class}: #{e.message}\n#{e.backtrace.join("\n")}."
    exit 1
end
exit 0
editor = options[:editor]
if !options[:editor]
    editor = ENV['EDITOR']
    if !editor
        puts "Please, create EDITOR environment variable -> export EDITOR=your favorite editor"
        exit 1
    end
end

if options[:create]
    actions = ActionParser.new
    actions.create_json(editor)
    actions_json
end

if options[:remove]
    puts "Removing new task..."
end
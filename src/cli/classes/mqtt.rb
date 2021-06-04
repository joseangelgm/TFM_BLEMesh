require 'timeout'

class MQTT

    DEFAULT_TIME = 5
    private_constant :DEFAULT_TIME

    attr_reader :response, :time_to_wait

    public

    def initialize(config_mqtt)
        # An object is blank if it’s false, empty, or a whitespace string. For example, "", " ", nil, [], and {} are all blank.
        raise Exception, "Missing ip param. Is empty. Check config.yaml"                 if !config_mqtt.key? :ip                 || !config_mqtt[:ip]
        raise Exception, "Missing port param. Is empty. Check config.yaml"               if !config_mqtt.key? :port               || !config_mqtt[:port]
        raise Exception, "Missing subscription topic param, is empty. Check config.yaml" if !config_mqtt.key? :topics             || !config_mqtt[:topics]
        raise Exception, "Missing subscription topic param, is empty. Check config.yaml" if !config_mqtt[:topics].key? :topic_pub || !config_mqtt[:topics][:topic_pub]
        raise Exception, "Missing publish topic param, is empty. Check config.yaml"      if !config_mqtt[:topics].key? :topic_sub || !config_mqtt[:topics][:topic_sub]

        @ip           = config_mqtt[:ip]
        @port         = config_mqtt[:port]
        @topic_pub    = config_mqtt[:topics][:topic_pub]
        @topic_sub    = config_mqtt[:topics][:topic_sub]
        @time_to_wait = config_mqtt[:time_to_wait_response] || DEFAULT_TIME
        @client       = PahoMqtt::Client.new({:host => @ip, :port => @port, :ssl => false})
        @response     = nil

    end

    def send_json(json)
        puts "Sending json over mqtt..."
        ### Register a callback on message event to display messages
        wait_message = true
        @client.on_message do |message|
            @response = JSON.parse(message.payload)
            wait_message = false
        end

        ### Register a callback on suback to assert the subcription
        waiting_suback = true
        @client.on_suback do
            waiting_suback = false
        end

        ### Connect to the eclipse test server on port 1883 (Unencrypted mode)
        @client.connect

        ### Subscribe to a topic
        @client.subscribe(@topic_sub, 1) # accept a list with [topic, qos]

        ### Waiting for the suback answer and excute the previously set on_suback callback
        while waiting_suback do
            sleep 0.001
        end

        ### Publlish a message on the topic @topic with "retain == false" and "qos == 1"
        @client.publish(@topic_pub, JSON[json], false, 1)

        Timeout::timeout(@time_to_wait) do
            puts "Waiting response...".bold.yellow
            while wait_message do
                sleep 0.001
            end
        end

        ### Calling an explicit disconnect
        @client.disconnect
    end
end
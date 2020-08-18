// This sample code is intended to show that the Connext DDS Micro 2.4.12
// C API can be called from a C++ application. It is STRICTLY an example and
// NOT intended to represent production-quality code.

#include <stdio.h>

// headers from Connext DDS Micro installation
#include "rti_me_c.h"
#include "disc_dpse/disc_dpse_dpsediscovery.h"
#include "wh_sm/wh_sm_history.h"
#include "rh_sm/rh_sm_history.h"
#include "netio/netio_udp.h"

// rtiddsgen generated headers
#include "example.h"
#include "examplePlugin.h"
#include "exampleSupport.h"

// DPSE Discovery-related constants defined in this header
#include "discovery_constants.h"


int main(void)
{
    // user-configurable values
    char *peer = "127.0.0.1";
    char *loopback_name = "Loopback Pseudo-Interface 1";
    char *eth_nic_name = "Wireless LAN adapter Wi-Fi";
    int domain_id = 100;

    DDS_DomainParticipantFactory *dpf = NULL;
    struct DDS_DomainParticipantFactoryQos dpf_qos = 
            DDS_DomainParticipantFactoryQos_INITIALIZER;
    DDS_DomainParticipant *dp = NULL;
    struct DDS_DomainParticipantQos dp_qos = 
            DDS_DomainParticipantQos_INITIALIZER;
    struct DPSE_DiscoveryPluginProperty discovery_plugin_properties =
            DPSE_DiscoveryPluginProperty_INITIALIZER;
    RT_Registry_T *registry = NULL;
    struct UDP_InterfaceFactoryProperty *udp_property = NULL;
    DDS_Publisher *publisher = NULL;

    // Related to topic_one
    DDS_Topic *topic_one = NULL;
    DDS_DataWriter *dw_one = NULL;
    type_oneDataWriter *narrowed_dw_one = NULL;
    type_one *type_one_sample = NULL;

    // Related to topic_two
    DDS_Topic *topic_two = NULL;
    DDS_DataWriter *dw_two = NULL;
    type_twoDataWriter *narrowed_dw_two = NULL;
    type_two *type_two_sample = NULL;

    // These structs can be reused when setting up both of our DataWriters, as 
    // well as the remote subscriptions we are going to assert.
    struct DDS_DataWriterQos dw_qos = DDS_DataWriterQos_INITIALIZER;
    struct DDS_SubscriptionBuiltinTopicData rem_subscription_data =
            DDS_SubscriptionBuiltinTopicData_INITIALIZER;

    DDS_Entity *entity = NULL;
    int sample_count = 0;
    DDS_ReturnCode_t retcode;
    DDS_Boolean success = DDS_BOOLEAN_FALSE;

    // create the DomainParticipantFactory and registry so that we can make some 
    // changes to the default values
    dpf = DDS_DomainParticipantFactory_get_instance();
    registry = DDS_DomainParticipantFactory_get_registry(dpf);

    // register writer history
    if (!RT_Registry_register(
            registry, 
            DDSHST_WRITER_DEFAULT_HISTORY_NAME,
            WHSM_HistoryFactory_get_interface(), 
            NULL, 
            NULL))
    {
        printf("ERROR: failed to register wh\n");
    }
    // register reader history
    if (!RT_Registry_register(
            registry, 
            DDSHST_READER_DEFAULT_HISTORY_NAME,
            RHSM_HistoryFactory_get_interface(), 
            NULL, 
            NULL))
    {
        printf("ERROR: failed to register rh\n");
    }

    // Set up the UDP transport's allowed interfaces. To do this we:
    // (1) unregister the UDP trasport
    // (2) name the allowed interfaces
    // (3) re-register the transport
    if(!RT_Registry_unregister(
            registry, 
            NETIO_DEFAULT_UDP_NAME, 
            NULL, 
            NULL)) 
    {
        printf("ERROR: failed to unregister udp\n");
    }

    udp_property = (struct UDP_InterfaceFactoryProperty *)
            malloc(sizeof(struct UDP_InterfaceFactoryProperty));
    if (udp_property == NULL) {
        printf("ERROR: failed to allocate udp properties\n");
    }
    *udp_property = UDP_INTERFACE_FACTORY_PROPERTY_DEFAULT;

    // For additional allowed interface(s), increase maximum and length, and
    // set interface below:
    REDA_StringSeq_set_maximum(&udp_property->allow_interface,2);
    REDA_StringSeq_set_length(&udp_property->allow_interface,2);
    *REDA_StringSeq_get_reference(&udp_property->allow_interface,0) = 
            DDS_String_dup(loopback_name); 
    *REDA_StringSeq_get_reference(&udp_property->allow_interface,1) = 
            DDS_String_dup(eth_nic_name); 

#if 0  

    // When you are working on an RTOS or other lightweight OS, the middleware
    // may not be able to get the NIC information automatically. In that case, 
    // the code below can be used to manually tell the middleware about an 
    // interface. The interface name ("en0" below) could be anything, but should
    // match what you included in the "allow_interface" calls above.

    if (!UDP_InterfaceTable_add_entry(
    		&udp_property->if_table,
            0xc0a864c8,	// IP address of 192.168.100.200
			0xffffff00, // netmask of 255.255.255.0
			"en0",
			UDP_INTERFACE_INTERFACE_UP_FLAG |
			UDP_INTERFACE_INTERFACE_MULTICAST_FLAG)) {

    	LOG(1, "failed to add interface")

    }

#endif

    if(!RT_Registry_register(
            registry, 
            NETIO_DEFAULT_UDP_NAME,
            UDP_InterfaceFactory_get_interface(),
            (struct RT_ComponentFactoryProperty*)udp_property, NULL))
    {
        printf("ERROR: failed to re-register udp\n");
    } 

    // register the dpse (discovery) component
    if (!RT_Registry_register(
            registry,
            "dpse",
            DPSE_DiscoveryFactory_get_interface(),
            &discovery_plugin_properties._parent, 
            NULL))
    {
        printf("ERROR: failed to register dpse\n");
    }

    // Now that we've finsihed the changes to the registry, we will start 
    // creating DDS entities. By setting autoenable_created_entities to false 
    // until all of the DDS entities are created, we limit all dynamic memory 
    // allocation to happen *before* the point where we enable everything.
    DDS_DomainParticipantFactory_get_qos(dpf, &dpf_qos);
    dpf_qos.entity_factory.autoenable_created_entities = DDS_BOOLEAN_FALSE;
    DDS_DomainParticipantFactory_set_qos(dpf, &dpf_qos);

    // configure discovery prior to creating our DomainParticipant
    if(!RT_ComponentFactoryId_set_name(&dp_qos.discovery.discovery.name, "dpse")) {
        printf("ERROR: failed to set discovery plugin name\n");
    }
    if(!DDS_StringSeq_set_maximum(&dp_qos.discovery.initial_peers, 1)) {
        printf("ERROR: failed to set initial peers maximum\n");
    }
    if (!DDS_StringSeq_set_length(&dp_qos.discovery.initial_peers, 1)) {
        printf("ERROR: failed to set initial peers length\n");
    }
    *DDS_StringSeq_get_reference(&dp_qos.discovery.initial_peers, 0) = 
            DDS_String_dup(peer);

    // configure the DomainParticipant's resource limits... these are just 
    // examples, if there are more remote or local endpoints these values would
    // need to be increased
    dp_qos.resource_limits.max_destination_ports = 4;
    dp_qos.resource_limits.max_receive_ports = 4;
    dp_qos.resource_limits.local_topic_allocation = 2;
    dp_qos.resource_limits.local_type_allocation = 2;
    dp_qos.resource_limits.local_reader_allocation = 1;
    dp_qos.resource_limits.local_writer_allocation = 2;
    dp_qos.resource_limits.remote_participant_allocation = 2;
    dp_qos.resource_limits.remote_reader_allocation = 2;
    dp_qos.resource_limits.remote_writer_allocation = 2;

    // set the name of the local DomainParticipant (i.e. - this application) 
    // from the constants defined in discovery_constants.h
    // (this is required for DPSE discovery)
    strcpy(dp_qos.participant_name.name, k_PARTICIPANT01_NAME);

    // now the DomainParticipant can be created
    dp = DDS_DomainParticipantFactory_create_participant(
            dpf, 
            domain_id,
            &dp_qos, 
            NULL,
            DDS_STATUS_MASK_NONE);
    if(dp == NULL) {
        printf("ERROR: failed to create participant\n");
    }

    // register both of the types that we will use (type_one and type_two, from 
    // the idl) with the middleware
    retcode = DDS_DomainParticipant_register_type(
            dp,
            type_oneTypePlugin_get_default_type_name(),
            type_oneTypePlugin_get());
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to register type\n");
    }
    retcode = DDS_DomainParticipant_register_type(
            dp,
            type_twoTypePlugin_get_default_type_name(),
            type_twoTypePlugin_get());
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to register type\n");
    }

    // Create both of the Topics that this publishing application will use.
    // Note that the names of the Topics are stored in const strings defined 
    // in the IDL 
    topic_one = DDS_DomainParticipant_create_topic(
            dp,
            topic_one_name, // this constant is defined in the *.idl file
            type_oneTypePlugin_get_default_type_name(),
            &DDS_TOPIC_QOS_DEFAULT, 
            NULL,
            DDS_STATUS_MASK_NONE);
    if(topic_one == NULL) {
        printf("ERROR: topic_one == NULL\n");
    }
    topic_two = DDS_DomainParticipant_create_topic(
            dp,
            topic_two_name, // this constant is defined in the *.idl file
            type_twoTypePlugin_get_default_type_name(),
            &DDS_TOPIC_QOS_DEFAULT, 
            NULL,
            DDS_STATUS_MASK_NONE);
    if(topic_two == NULL) {
        printf("ERROR: topic_two == NULL\n");
    }

    // assert the 2 remote DomainParticipants (whos names are defined in 
    // discovery_constants.h) that we are expecting to discover
    retcode = DPSE_RemoteParticipant_assert(dp, k_PARTICIPANT02_NAME);
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote participant\n");
    }
    retcode = DPSE_RemoteParticipant_assert(dp, k_PARTICIPANT03_NAME);
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote participant\n");
    }

    // create the Publisher
    publisher = DDS_DomainParticipant_create_publisher(
            dp,
            &DDS_PUBLISHER_QOS_DEFAULT,
            NULL,
            DDS_STATUS_MASK_NONE);
    if(publisher == NULL) {
        printf("ERROR: Publisher == NULL\n");
    }

    // Configure QoS for dw_one. Note that the 'rtps_object_id' that we 
    // assign to this DataWriter needs to be the same number any remote, 
    // matching DataReader will configure for its remote peer. We are defining 
    // these IDs and other constants in discovery_constants.h
    dw_qos.protocol.rtps_object_id = k_OBJ_ID_PARTICIPANT01_DW01;
    dw_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
    dw_qos.writer_resource_limits.max_remote_readers = 1;
    dw_qos.resource_limits.max_samples_per_instance = 8;
    dw_qos.resource_limits.max_instances = 2;
    dw_qos.resource_limits.max_samples = dw_qos.resource_limits.max_instances *
            dw_qos.resource_limits.max_samples_per_instance;
    dw_qos.history.depth = 8;
    dw_qos.protocol.rtps_reliable_writer.heartbeat_period.sec = 0;
    dw_qos.protocol.rtps_reliable_writer.heartbeat_period.nanosec = 250000000;

    // now create dw_one
    dw_one = DDS_Publisher_create_datawriter(
            publisher, 
            topic_one, 
            &dw_qos,
            NULL,
            DDS_STATUS_MASK_NONE);
    if(dw_one == NULL) {
        printf("ERROR: dw_one == NULL\n");
    }

    // A DDS_DataWriter is not type-specific, thus we need to cast, or "narrow"
    // the DataWriter before we use it to write our samples
    narrowed_dw_one = type_oneDataWriter_narrow(dw_one);

    // create the data sample that we will write
    type_one_sample = type_one_create();
    if(type_one_sample == NULL) {
        printf("ERROR: failed type_one_create\n");
    }

    // Configure QoS for dw_two. Note that we are able to reuse the "dw_qos"
    // struct because no memory is loaned during the creation of the DW. Also 
    // note that the 'rtps_object_id' has changed compared to dw_one.
    dw_qos.protocol.rtps_object_id = k_OBJ_ID_PARTICIPANT01_DW02;
    dw_qos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
    dw_qos.writer_resource_limits.max_remote_readers = 1;
    dw_qos.resource_limits.max_samples_per_instance = 8;
    dw_qos.resource_limits.max_instances = 2;
    dw_qos.resource_limits.max_samples = dw_qos.resource_limits.max_instances *
            dw_qos.resource_limits.max_samples_per_instance;
    dw_qos.history.depth = 8;
    dw_qos.protocol.rtps_reliable_writer.heartbeat_period.sec = 0;
    dw_qos.protocol.rtps_reliable_writer.heartbeat_period.nanosec = 250000000;

    // now create dw_two and narrowed_dw_two
    dw_two = DDS_Publisher_create_datawriter(
            publisher, 
            topic_two, 
            &dw_qos,
            NULL,
            DDS_STATUS_MASK_NONE);
    if(dw_two == NULL) {
        printf("ERROR: dw_one == NULL\n");
    }
    narrowed_dw_two = type_twoDataWriter_narrow(dw_two);

    // create the data sample that we will write for topic_two
    type_two_sample = type_two_create();
    if(type_two_sample == NULL) {
        printf("ERROR: failed type_two_create\n");
    }

    // When we use DPSE discovery we must mannually setup information about any 
    // DataReaders we are expecting to discover, and assert them. This 
    // information includes a unique  object ID for the remote peer (we are 
    // defining this in  discovery_constants.h), as well as its Topic, type, 
    // and QoS. 

    // first remote DataReader
    rem_subscription_data.key.value[DDS_BUILTIN_TOPIC_KEY_OBJECT_ID] = 
            k_OBJ_ID_PARTICIPANT02_DR01;
    rem_subscription_data.topic_name = DDS_String_dup(topic_one_name);
    rem_subscription_data.type_name = 
            DDS_String_dup(type_oneTypePlugin_get_default_type_name());
    rem_subscription_data.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

    // Assert that a remote DomainParticipant (with the name held in
    // k_PARTICIANT02_NAME) will contain a DataReader described by the 
    // information in the rem_subscription_data struct.
    retcode = DPSE_RemoteSubscription_assert(
            dp,
            k_PARTICIPANT02_NAME,
            &rem_subscription_data,
            type_one_get_key_kind(type_oneTypePlugin_get(), NULL));
    if (retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote publication\n");
    }  

    // second remote DataReader
    rem_subscription_data.key.value[DDS_BUILTIN_TOPIC_KEY_OBJECT_ID] = 
            k_OBJ_ID_PARTICIPANT03_DR01;
    rem_subscription_data.topic_name = DDS_String_dup(topic_two_name);
    rem_subscription_data.type_name = 
            DDS_String_dup(type_twoTypePlugin_get_default_type_name());
    rem_subscription_data.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

    // Assert that a remote DomainParticipant (with the name held in
    // k_PARTICIANT03_NAME) will contain a DataReader described by the 
    // information in the rem_subscription_data struct.
    retcode = DPSE_RemoteSubscription_assert(
            dp,
            k_PARTICIPANT03_NAME,
            &rem_subscription_data,
            type_two_get_key_kind(type_twoTypePlugin_get(), NULL));
    if (retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to assert remote publication\n");
    }    

    // Finaly, now that all of the entities are created, we can enable them all
    entity = DDS_DomainParticipant_as_entity(dp);
    retcode = DDS_Entity_enable(entity);
    if(retcode != DDS_RETCODE_OK) {
        printf("ERROR: failed to enable entity\n");
    }

    while (1) {
        
        // add some arbitrary data to the samples then write them...

        type_one_sample->id = 1;
        sprintf(type_one_sample->msg, "sample #%d\n", sample_count);

        retcode = type_oneDataWriter_write(
                narrowed_dw_one, 
                type_one_sample, 
                &DDS_HANDLE_NIL);
        if(retcode != DDS_RETCODE_OK) {
            printf("ERROR: Failed to write sample\n");
        } else {
            printf("INFO: [topic_one] Wrote sample %d\n", sample_count); 
        } 

        type_two_sample->id = 1;
        type_two_sample->x = sample_count;
        type_two_sample->y = sample_count + 1;
        type_two_sample->z = sample_count + 2;

        retcode = type_twoDataWriter_write(
                narrowed_dw_two, 
                type_two_sample, 
                &DDS_HANDLE_NIL);
        if(retcode != DDS_RETCODE_OK) {
            printf("ERROR: Failed to write sample\n");
        } else {
            printf("INFO: [topic_two] Wrote sample %d\n", sample_count); 
        } 

        sample_count++;
        OSAPI_Thread_sleep(1000); // sleep 1s between writes 
    }
}


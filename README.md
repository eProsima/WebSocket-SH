<a href="https://integration-service.docs.eprosima.com/"><img src="https://github.com/eProsima/Integration-Service/blob/main/docs/images/logo.png?raw=true" hspace="8" vspace="2" height="100" ></a>

# ROS 1 System Handle

## Introduction

### What is a System Handle?
[![WebSocket SH CI Status](https://github.com/eProsima/WebSocket-SH/actions/workflows/ci.yml/badge.svg)](https://github.com/eProsima/WebSocket-SH/actions)

A [System Handle](<!--TODO: add link-->) is a plugin that allows a certain middleware
or communication protocol to speak the same language used by the [eProsima Integration Service](https://github.com/eProsima/Integration-Service),
that is, *Extensible and Dynamic Topic Types for DDS* (**xTypes**);
specifically, *Integration Service* bases its intercommunication abilities on eProsima's open source
implementation for the *xTypes* protocol, that is, [eProsima xTypes](https://github.com/eProsima/xtypes).

![System Handle Architecture](docs/images/system-handle-architecture.png)

### The WebSocket SystemHandle

<a href="https://docs.ros.org/en/noetic/"><img src="docs/images/websocket_logo.png" align="left" hspace="8" vspace="2" width="90"></a>

This repository contains the source code of *Integration Service* **System Handle**
for the [ROS 1](https://wiki.ros.org/noetic) middleware protocol, widely used in the robotics field.

The main purpose of the *ROS 1 System Handle* is that of establishing a connection between a *ROS 1*
application and an application running over a different middleware implementation.
This is the classic use-case approach for *Integration Service*.

## Configuration

*Integration Service* is configured by means of a YAML configuration file, which specifies
the middlewares, topics and/or services involved in the intercommunication process, as well as
their topic/service types and the data exchange flow. This configuration file is loaded during
runtime, so there is no need to recompile any package before switching to a whole new
intercommunication architecture.

To get a more precise idea on how these YAML files have to be filled and which fields they require
in order to succesfully configure and launch an *Integration Service* project, please refer to the
dedicated [configuration](<!-- TODO: add link -->) section of the official documentation.
An illustrative explanation is also presented in the *Readme* `Configuration` section of the
[general project repository](https://github.com/eProsima/Integration-Service).

Regarding the *ROS 1 System Handle*, there are several specific parameters which can be configured
for the ROS 1 middleware. All of these parameters are optional, and fall as suboptions of the main
five sections described in the *Configuration* chapter of *Integration Service* repository:

* `systems`: The system `type` must be `ros1`. In addition to the `type` and `types-from` fields,
  the *ROS 1 System Handle* accepts the following specific configuration fields:

  ```yaml
  systems:
    ros1:
      type: ros1
      node_name: "my_ros1_node"
  ```
  * `node_name`: The *ROS 1 System Handle* node name.

* `topics`: The topic `route` must contain `ros1` within its `from` or `to` fields. Additionally,
  the *ROS 1 System Handle* accepts the following topic specific configuration parameters, within the
  `ros1` specific middleware configuration tag:

  ```yaml
  routes:
    ros2_to_ros1: { from: ros2, to: ros1 }
    ros1_to_dds: { from: ros1, to: dds }

  topics:
    hello_ros1:
      type: std_msgs/String
      route: ros2_to_ros1
      ros1: { queue_size: 10, latch: false }
    hello_dds:
      type: std_msgs/String
      route: ros1_to_dds
      ros1: { queue_size: 5 }
  ```

  * `queue_size`: The maximum message queue size for the ROS 1 publisher or subscription.
  * `latch`: Enable or disable latching. When a connection is latched,
    the last message published is saved and sent to any future subscribers that connect.
    This configuration parameter only makes sense for ROS 1 publishers, so it is only useful for
    routes where the *ROS 1 System Handle* acts as a publisher, that is, for routes where `ros1` is
    included in the `to` list.
## Examples

There are several *Integration Service* examples using the *ROS 1 System Handle* available
in the project's [main source code repository]([https://](https://github.com/eProsima/Integration-Service/tree/main/examples)).

Some of these examples, where the *ROS 1 System Handle* plays a different role in each of them, are introduced here.

### Publisher/subscriber intercommunication between ROS 1 and ROS 2

In this example, *Integration Service* uses both this *ROS 1 System Handle* and the *ROS 2 System Handle*
to transmit data coming from a ROS 1 publisher into the ROS 2 data space, so that it can be
consumed by a ROS 2 subscriber on the same topic, and viceversa.

The configuration file used by *Integration Service* for this example can be found
[here](https://github.com/eProsima/Integration-Service/blob/main/examples/basic/ros1_ros2__helloworld.yaml).

For a detailed step by step guide on how to build and test this example, please refer to the
[official documentation](<!-- TODO: link to example -->).

<!-- TODO: add YAML and applications for DDS and ROS2 to test this
### ROS 1 service server addressing petitions coming from a ROS 2 service client

The configuration file for this example can be found
[here](TODO).

Below, a high level diagram is presented, showing which entities will *Integration Service* create
to forward the petitions requested from a ROS 2 client application to a ROS 1 service server application,
which will process them and produce a reply message which will be transmited back to the DDS client:

![ROS1_server_and_ROS2_client](TODO)

For a detailed step by step guide on how to build and test this example, please refer to the
[official documentation](TODO: link).
-->
## Compilation flags

Besides the [global compilation flags](<!-- TODO: link to IS readme section-->) available for the
whole *Integration Service* product suite, there are some specific flags which apply only to the
*ROS 1 System Handle*; they are listed below:

* `BUILD_ROS1_TESTS`: Allows to specifically compile the *ROS 1 System Handle* unitary and
  integration tests; this is useful to avoid compiling each *System Handle's* test suite present
  in the `colcon` workspace, which is what would happen if using the `BUILD_TESTS` flag; and thus,
  minimizing the building time; to use it, after making sure that the *ROS 1 System Handle*
  is present in the `colcon` workspace, the following command must be executed:
  ```bash
  ~/is_ws$ colcon build --cmake-args -DBUILD_ROS1_TESTS=ON
  ```

* `MIX_ROS_PACKAGES`: It accepts as an argument a list of [ROS 1 packages](https://index.ros.org/packages/),
  such as `std_msgs`, `geometry_msgs`, `sensor_msgs`, `nav_msgs`... for which the required transformation
  library to convert the specific ROS 1 type definitions into *xTypes*, and the other way around, will be built.
  This library is also known within the *Integration Service* context as `Middleware Interface Extension`
  or `mix` library.

  By default, only the `std_msgs_mix` library is compiled, unless the `BUILD_TESTS`
  or `BUILD_ROS1_TESTS` is used, case in which some additional ROS 1 packages `mix` files
  required for testing will be built.

  If an user wants to compile some additional packages to use them with *Integration Service*,
  the following command must be launched to compile it, adding as much packages to the list as desired:
  ```bash
  ~/is_ws$ colcon build --cmake-args -DMIX_ROS_PACKAGES="std_msgs geometry_msgs sensor_msgs nav_msgs"
  ```

<!-- TODO: complete when it is uploaded to read the docs
## API Reference
-->

## License

This repository is open-sourced under the *Apache-2.0* license. See the [LICENSE](LICENSE) file for more details.

## Getting help

If you need support you can reach us by mail at `support@eProsima.com` or by phone at `+34 91 804 34 48`.



# Integration Service System Handle for WebSocket

Based on `websocketpp` C++ implementation for the WebSocket Protocol.

## Configuration

The following fields are available:

*type*: REQUIRED. MUST be `websocket_server` to use this plugin.

*port*: REQUIRED. The port to listen on.

*cert*: REQUIRED. x509 cert the server should use. Currently the server only supports wss protocol so this field is required.

*key*: REQUIRED. private key corresponding to the cert.

*authentication*: see [Authentication Options](#Authentication-Options)

Example:
```yaml
systems:
  client:
    type: websocket_server
    port: 50001
    cert: path/to/file.crt
    key: path/to/file.key
    authentication: { secret: my_secret }

...other Integration Service options
```

Paths for cert and keys can be either absolute, relative to the config file or relative to your home directory.

### Authentication Options

The authentication option is a map with the following keys:

*secret*: The secret to use for MAC based verification.

*pubkey*: Path to a file containing a PEM encoded public key.

Either a *secret* or *pubkey* must be present.

*rules*: List of additional claims that should be checked. It should contain a map with keys corresponding to the claim identifier and value a glob pattern.

For example, this rule will check if the payload contains a `myClaim` claim and that it's value is exactly equal to `expectedValue`.
```yaml
rules: { myClaim: expectedValue }
```
And this rule will check that `myClaim` contains any value that starts will `foo`
```yaml
rules: { myClaim: foo* }
```

*policies*: An array of authentication options to check, a token is allowed if it passes any of the policies. Each policy is an authentication option, that is, it should have the same fields as described above (*secret*, *pubkey*, *rules* etc). If this field is present, the default policy will not be used.

For example, this will allow a token that is signed with `my_secret` OR a token that is signed with `my_secret_2` and has a `myClaim` claim with the value `myValue`.
```yaml
policies: [
  {
    secret: my_secret,
  },
  {
    secret: my_secret_2,
    rules: { myClaim: myValue },
  },
]
```
In this example, the default policy here will NOT be used, so this will only accept tokens signed with `my_secret_2`.
```yaml
secret: my_secret # not used!
policies: [ { secret: my_secret_2 }]
```

# Aerospike Node.js Client [![travis][travis-image]][travis-url] [![codecov][codecov-image]][codecov-url] [![npm][npm-image]][npm-url] [![downloads][downloads-image]][downloads-url]

[travis-image]: https://travis-ci.org/aerospike/aerospike-client-nodejs.svg?branch=master
[travis-url]: https://travis-ci.org/aerospike/aerospike-client-nodejs
[codecov-image]: https://codecov.io/gh/aerospike/aerospike-client-nodejs/branch/master/graph/badge.svg
[codecov-url]: https://codecov.io/gh/aerospike/aerospike-client-nodejs
[npm-image]: https://img.shields.io/npm/v/aerospike.svg
[npm-url]: https://www.npmjs.com/package/aerospike
[downloads-image]: https://img.shields.io/npm/dm/aerospike.svg
[downloads-url]: http://npm-stat.com/charts.html?package=aerospike

The Aerospike Node.js client is a Node.js add-on module, written using V8.

The client is compatible with Node.js 19, 18 (LTS), 16 (LTS) and 14 (LTS).
It supports the following operating systems:
- RHEL 8/9
- Debian 10 (x86_64 architecture only)
- Debian 11
- Ubuntu 20.04/22.04 (Focal Fossa, Jammy Jellyfish)
- Many Linux distributions compatible with one of the above OS releases.
- macOS versions 11/12/13 are also supported. (Node.js 14 install unavailable on M1 Mac systems)

The client is compatible with arm64, aarch64, and x86_64 architectures.

The Aerospike Node.js client supports all Node.js [LTS
releases](https://github.com/nodejs/Release#release-schedule). To download and
install the latest stable version of Node.js, visit
[nodejs.org](http://nodejs.org/).

Install the necessary "development tools" and other libraries to build the client software. 
Reference various docker files in the repository under the docker directory for more information.

## Installation

The Aerospike Node.js client is an add-on module that uses the Aerospike C client. The installation will attempt to install the pre-built binaries including dependent C client.

You can install the Aerospike Node.js client like any other Node.js module.

### Primer on Node.js Modules

Node.js modules are containers of JavaScript code and a `package.json`, which defines
the module, its dependencies and requirements. Modules are usually installed as
dependencies of other Node.js applications or modules. The modules are installed in
the application's `node_modules` directory, and can be utilized within the program
by requiring the module by name.

### npm Registry Installations

To install `aerospike` as a dependency of your project, in your project directory run:

```bash
npm install aerospike
```

To add `aerospike` as a dependency in _package.json_, run:

```bash
npm install aerospike --save-dev
```

To require the module in your application:
```bash
const Aerospike = require('aerospike')
```

## Usage

The following is very simple example how to create, update, read and remove a
record using the Aerospike database.

```js
const Aerospike = require('aerospike')

// INSERT HOSTNAME AND PORT NUMBER OF AEROPSIKE SERVER NODE HERE!
const config = {
  hosts: '192.168.33.10:3000',
}

const key = new Aerospike.Key('test', 'demo', 'demo')

Aerospike.connect(config)
  .then(client => {
    const bins = {
      i: 123,
      s: 'hello',
      b: Buffer.from('world'),
      d: new Aerospike.Double(3.1415),
      g: Aerospike.GeoJSON.Point(103.913, 1.308),
      l: [1, 'a', {x: 'y'}],
      m: {foo: 4, bar: 7}
    }
    const meta = { ttl: 10000 }
    const policy = new Aerospike.WritePolicy({
      exists: Aerospike.policy.exists.CREATE_OR_REPLACE,
      // Timeouts disabled, latency dependent on server location. Configure as needed.
      socketTimeout : 0, 
      totalTimeout : 0
    })

    return client.put(key, bins, meta, policy)
      .then(() => {
        const ops = [
          Aerospike.operations.incr('i', 1),
          Aerospike.operations.read('i'),
          Aerospike.lists.append('l', 'z'),
          Aerospike.maps.removeByKey('m', 'bar')
        ]

        return client.operate(key, ops)
      })
      .then(result => {
        console.log(result.bins) // => { i: 124, l: 4, m: null }

        return client.get(key)
      })
      .then(record => {
        console.log(record.bins) // => { i: 124,
                                 //      s: 'hello',
                                 //      b: <Buffer 77 6f 72 6c 64>,
                                 //      d: 3.1415,
                                 //      g: '{"type":"Point","coordinates":[103.913,1.308]}',
                                 //      l: [ 1, 'a', { x: 'y' }, 'z' ],
                                 //      m: { foo: 4 } }
      })
      .then(() => client.close())
  })
  .catch(error => {
    console.error('Error: %s [%i]', error.message, error.code)
    if (error.client) {
      error.client.close()
    }
  })
```
## Prerequisites

#### RHEL/CentOS

To install library prerequisites using `yum`:

```bash
sudo yum group install "Development Tools" 
sudo yum install openssl openssl-devel
sudo yum install python3 python3-devel
```

#### Alpine Linux

```bash
apk add build-base \
    linux-headers \
    bash \
    libuv-dev \
    openssl-dev \
    lua5.1-dev \
    zlib-dev \
    git \
    python3
```

#### Amazon Linux

```bash
yum groupinstall "Development Tools"
yum install openssl openssl-devel
yum install python3 python3-devel
```

#### Debian

To install library prerequisites using `apt-get`:

```bash
sudo apt -y install software-properties-common
sudo apt -y install build-essential
sudo apt -y install libssl-dev
sudo apt -y install libarchive-dev cmake rsync curl libcurl4-openssl-dev zip
sudo apt -y install python3 python3-dev python3-pip
sudo apt install zlib1g-dev libncurses5-dev libgdbm-dev libnss3-dev \
    libsqlite3-dev libreadline-dev libffi-dev libbz2-dev -y
sudo apt -y install wget libtool m4 automake
```

#### Ubuntu

To install library prerequisites using `apt`:

```bash
sudo apt install g++ libssl libssl-dev zlib1g-dev
```

### macOS

Before starting with the Aerospike Nodejs Client, verify that you have the following prerequisites:

- macOS 10.8 or greater.
- Xcode 5 or greater.


**Openssl Library**

The below example shows how to install the Openssl library.

```bash
brew install openssl
unlink /usr/local/opt/openssl
# Change the below linking based on openssl version and installation path
ln -s /usr/local/Cellar/openssl@x/y.z/ /usr/local/opt/openssl
```

If you are looking upgrade switch from openssl&#64;1 or an older version of openssl&#64;3, add these variables to your .bashrc, .profile, or .zshrc file.
```bash
export PATH="/usr/local/bin/:/usr/local/opt/openssl/bin:$PATH"
export LDFLAGS="-L/usr/local/opt/openssl/lib"
export CPPFLAGS="-I/usr/local/opt/openssl/include"  
export EXT_CFLAGS="-I/usr/local/opt/openssl/include"
```

Afterwards, source the file that was changed and confirm the desired openssl version is installed
```bash
source ~/.bashrc
source ~/.profile
source ~/.zshrc
openssl version
```
For 4x client support, install openssl&#64;1.1 version.

**LIBUV Library**

The example below shows how to install the LIBUV library.

```bash
brew install libuv
unlink /usr/local/opt/libuv
# Change the below linking based on libuv version and installation path
ln -s /usr/local/Cellar/libuv/1.44.1_1/ /usr/local/opt/libuv
```

### Git Repository Installations

When using a cloned repository, install `aerospike` as a dependency of your application. Instead of referencing the module by name, you reference it by path.

To clone the repository use the following command:
```bash
  git clone --recursive git@github.com:aerospike/aerospike-client-nodejs.git
```

#### Building dependancy C client

Make sure to build the C client before doing npm install variants
Run the following commands to build the C client:
```bash
  ./scripts/build-c-client.sh
```

#### Building and installing Node.js client

To install the module as a dependency of your application, run the following in the application directory:

```bash
npm install --unsafe-perm --build-from-source
```

To require it in the application:

```bash
const Aerospike = require('aerospike')
```

## Documentation

Access the client API documentation at:
[https://docs.aerospike.com/apidocs/nodejs](https://docs.aerospike.com/apidocs/nodejs).
This documentation is built from the client's source using [JSDocs
v3](http://usejsdoc.org/index.html) for every release.

The API docs also contain a few basic tutorials:

* [Getting Started - Connecting to an Aerospike database cluster](https://www.aerospike.com/apidocs/nodejs/tutorial-getting_started.html)
* [Managing Aerospike connections in a Node cluster](https://www.aerospike.com/apidocs/nodejs/tutorial-node_clusters.html)
* [Handling asynchronous database operations using Callbacks, Promises or `async`/`await`](https://www.aerospike.com/apidocs/nodejs/tutorial-callbacks_promises_async_await.html)

A variety of additional example applications are provided in the
[`examples`](examples) directory of this repository.

Access backward incompatible API changes by a release at:
https://developer.aerospike.com/client/nodejs/usage/incompatible.

### API Versioning

The Aerospike Node.js client library follows [semantic versioning](http://semver.org/).
By Aerospike versioning guidelines, changes which may break backwards compatibility are
indicated by an increase in the major version number.
Minor, or patch releases, which are incremented only the
second and third version number, are always backwards compatible.


## Tests

The client includes a comprehensive test suite using
[Mocha](http://mochajs.org). The tests can be found in the repository under test directory.

Before running the tests, you need to update the dependencies:

    npm update

To run all the test cases:

    npm test

Note: make sure your server has TTL enabled for the `test` namespace ([Namespace Retention Configuration](https://docs.aerospike.com/server/operations/configure/namespace/retention)) to allow all tests to run correctly.

To run the tests and also report on test coverage:

    npm run coverage

## Benchmarks

Benchmark utilities are provided in the repository under benchmarks directory. See the benchmarks/README.md for details.

## License

The Aerospike Node.js Client is made available under the terms of the Apache
License, Version 2, as stated in the LICENSE file.

Individual files may be made available under their own specific license, all
compatible with Apache License, Version 2. Refer to individual files for
details.
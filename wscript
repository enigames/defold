#! /usr/bin/env python

VERSION = '0.1'
APPNAME = 'dlib'

srcdir = '.'
blddir = 'build'

import os, sys, waf_dynamo

def init():
    pass

def set_options(opt):
    opt.sub_options('src')
    opt.tool_options('compiler_cxx')

def configure(conf):
    conf.check_tool('compiler_cxx')
    conf.sub_config('src')

    waf_dynamo.configure(conf)

    if sys.platform == "darwin":
        platform = "darwin"
    elif sys.platform == "linux2":
        platform = "linux"
    elif sys.platform == "win32":
        platform = "win32"
    else:
        conf.fatal("Unable to determine platform")

    if platform == "win32":
        conf.env.append_value('CPPPATH', "../include/win32")

    dynamo_home = os.getenv('DYNAMO_HOME')
    if not dynamo_home:
        conf.fatal("DYNAMO_HOME not set")
    dynamo_ext = os.path.join(dynamo_home, "ext")

    conf.env.append_value('CPPPATH', os.path.join(dynamo_ext, "include"))
    conf.env.append_value('LIBPATH', os.path.join(dynamo_ext, "lib", platform))

    conf.env['LIB_GTEST'] = 'gtest'

    if sys.platform == "linux2":
        conf.env['LIB_THREAD'] = 'pthread'
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    elif platform == "darwin":
        conf.env['LIB_THREAD'] = ''
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    else:
        conf.env['LIB_THREAD'] = ''
        conf.env['LIB_PLATFORM_SOCKET'] = 'WS2_32'

def build(bld):
    bld.add_subdirs('src')
    bld.install_files('${PREFIX}/include/win32', 'include/win32/*.h')

def shutdown():
    waf_dynamo.run_gtests(valgrind = True)


/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#if USE(PLUGIN_HOST_PROCESS)

#import "NetscapePluginHostProxy.h"

#import <mach/mach.h>
#import "NetscapePluginHostManager.h"
#import "NetscapePluginInstanceProxy.h"
#import "WebKitPluginHost.h"

using namespace std;

namespace WebKit {

NetscapePluginHostProxy::NetscapePluginHostProxy(mach_port_t clientPort, mach_port_t pluginHostPort)
    : m_clientPort(clientPort)
    , m_pluginHostPort(pluginHostPort)
{
    // FIXME: We should use libdispatch for this.
    CFMachPortContext context = { 0, this, 0, 0, 0 };
    m_deadNameNotificationPort.adoptCF(CFMachPortCreate(0, deadNameNotificationCallback, &context, 0));

    mach_port_t previous;
    mach_port_request_notification(mach_task_self(), pluginHostPort, MACH_NOTIFY_DEAD_NAME, 0, 
                                   CFMachPortGetPort(m_deadNameNotificationPort.get()), MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
    ASSERT(previous == MACH_PORT_NULL);
    
    RetainPtr<CFRunLoopSourceRef> deathPortSource(AdoptCF, CFMachPortCreateRunLoopSource(0, m_deadNameNotificationPort.get(), 0));
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(), deathPortSource.get(), kCFRunLoopDefaultMode);
}

void NetscapePluginHostProxy::pluginHostDied()
{
    PluginInstanceSet instances;    
    m_instances.swap(instances);
  
    PluginInstanceSet::const_iterator end = instances.end();
    for (PluginInstanceSet::const_iterator it = instances.begin(); it != end; ++it)
        (*it)->pluginHostDied();
    
    NetscapePluginHostManager::shared().pluginHostDied(this);
    
    delete this;
}
    
void NetscapePluginHostProxy::addPluginInstance(PassRefPtr<NetscapePluginInstanceProxy> instance)
{
    ASSERT(!m_instances.contains(instance.get()));
    
    m_instances.add(instance);
}
    
void NetscapePluginHostProxy::removePluginInstance(NetscapePluginInstanceProxy* instance)
{
    ASSERT(!m_instances.contains(instance));

    m_instances.remove(instance);
}

void NetscapePluginHostProxy::deadNameNotificationCallback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    mach_msg_header_t* header = static_cast<mach_msg_header_t*>(msg);
    
    ASSERT(header && header->msgh_id == MACH_NOTIFY_DEAD_NAME);
    
    static_cast<NetscapePluginHostProxy*>(info)->pluginHostDied();
}

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS)

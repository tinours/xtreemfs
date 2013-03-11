/*
 * Copyright (c) 2012-2013 by Jens V. Fischer, Zuse Institute Berlin
 *               
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */

package org.xtreemfs.sandbox.benchmarkOSDPerformance;

import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicBoolean;

import org.xtreemfs.common.libxtreemfs.AdminClient;
import org.xtreemfs.common.libxtreemfs.ClientFactory;
import org.xtreemfs.foundation.logging.Logging;

/**
 * @author jensvfischer
 */
public class BenchmarkClientFactory {

    private static ConcurrentLinkedQueue<AdminClient> clients;

    /**
     * Create a AdminClient. The starting and shutdown of the client is managed by the BenchmarkClientFactory
     * (no need to call Client.start() and Client.shutdown())
     * 
     * @param connection the connection data
     * @return a started AdminClient instance
     * @throws Exception
     */
    static AdminClient getNewClient(ConnectionData connection) {
          return tryCreateClient(connection);
    }

    static {
        clients       = new ConcurrentLinkedQueue<AdminClient>();
        addShutdownHook();
    }

    /* error handling for 'createNewClient()" */
    private static AdminClient tryCreateClient(ConnectionData connection) {
        AdminClient client = null;
        try {
            client = createNewClient(connection);
        } catch (Exception e) {
            Logging.logMessage(Logging.LEVEL_ERROR, Logging.Category.tool, BenchmarkClientFactory.class, "Could not create new AdminClient. Errormessage: %s", e.getMessage());
            Thread.yield(); // allow logger to catch up
            e.printStackTrace();
            System.exit(42);
        }
        return client;
    }

    private static AdminClient createNewClient(ConnectionData connection) throws Exception {
        AdminClient client = ClientFactory.createAdminClient(connection.dirAddress, connection.userCredentials,
                connection.sslOptions, connection.options);
        clients.add(client);
        client.start();
        return client;
    }

    /* shutdown all clients add exit */
    private static void addShutdownHook() {
        Runtime.getRuntime().addShutdownHook(new Thread() {
            public void run() {
                for (AdminClient client : clients) {
                    client.shutdown();
                }
                Logging.logMessage(Logging.LEVEL_INFO, Logging.Category.tool, Runtime.getRuntime(),
                        "Shutting down %s clients", clients.size());
            }
        });
    }
}

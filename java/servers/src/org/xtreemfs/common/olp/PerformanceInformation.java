/*
 * Copyright (c) 2011 by Felix Langner,
 *               Zuse Institute Berlin
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */
package org.xtreemfs.common.olp;

import org.xtreemfs.foundation.buffer.BufferPool;
import org.xtreemfs.foundation.buffer.ReusableBuffer;

/**
 * <p>Instances of this class encapsulate performance measurements that are exchanged between different stages. It also
 * provides methods to serialize and dezerialize these informations to be able to share them via RPCs.</p>
 * 
 * @author flangner
 * @since 09/01/2011
 * @version 1.0
 */
public class PerformanceInformation {

    /**
     * <p>Unique identifier for the stage providing this performance information.</p>
     */
    int         id;
    
    /**
     * <p>The fixed processing time averages to be expected at this stage in ms.</p>
     */
    double[]    fixedProcessingTimeAverages;
    
    /**
     * <p>The variable processing time averages to be expected at this stage in ms/byte.</p>
     */
    double[]    variableProcessingTimeAverages;
    
    /**
     * <p>The idle time in ms retrieved from size and composition of this and subsequent queues for the remaining 
     * queuing network.</p>
     */
    double      idleTime;

    /**
     * <p>Constructor to bundle important performance information to be ready for forwarding to the preceding stage.</p>
     * 
     * @param id - unique identifier for this stage.
     * @param fixedProcessingTimeAverages - the fixed processing time averages at this stage in ms.
     * @param variableProcessingTimeAverages - the variable processing time averages at this stage in ms/byte.
     * @param idleTime - the idle time retrieved from size and composition of this and subsequent queues for the 
     *                   remaining queuing network.
     */
    PerformanceInformation(int id, double[] fixedProcessingTimeAverages, double[] variableProcessingTimeAverages, 
            double idleTime) {
                
        assert(fixedProcessingTimeAverages.length == variableProcessingTimeAverages.length);
        
        this.id = id;
        this.fixedProcessingTimeAverages = fixedProcessingTimeAverages;
        this.variableProcessingTimeAverages = variableProcessingTimeAverages;
        this.idleTime = idleTime;
    }
    
    /**
     * <p>Constructor to unpack serialized {@link PerformanceInformation}.</p>
     * 
     * @param serialized - packed {@link PerformanceInformation}.
     */
    PerformanceInformation(ReusableBuffer serialized) {
        
        dezerialize(serialized, this);
    }
    
    /**
     * @return a serialized representation of the performance information given by this object.
     */
    public ReusableBuffer serialize() {
        
        int length = fixedProcessingTimeAverages.length;
        ReusableBuffer result = BufferPool.allocate((Integer.SIZE * 2 + Double.SIZE * ((length * 2) + 1)) / 8);
        result.putInt(id);
        result.putInt(length);
        for (int i = 0; i < length; i++) {
            result.putDouble(fixedProcessingTimeAverages[i]);
            result.putDouble(variableProcessingTimeAverages[i]);
        }
        result.putDouble(idleTime);
        result.flip();
        return result;
    }
    
/*
 * private methods    
 */
    
    /**
     * <p>Method to retrieve {@link PerformanceInformation} from the serialized representation and store them at the 
     * given target.</p>
     * 
     * @param serialized - packed representation of a {@link PerformanceInformation}.
     * @param target - object to be filled with the information retrieved.
     */
    private static void dezerialize(ReusableBuffer serialized, PerformanceInformation target) {
        
        target.id = serialized.getInt();
        int length = serialized.getInt();
        target.fixedProcessingTimeAverages = new double[length];
        target.variableProcessingTimeAverages = new double[length];
        for (int i = 0; i < length; i++) {
            target.fixedProcessingTimeAverages[i] = serialized.getDouble();
            target.variableProcessingTimeAverages[i] = serialized.getDouble();
        }
        target.idleTime = serialized.getDouble();
        serialized.flip();
    }
}
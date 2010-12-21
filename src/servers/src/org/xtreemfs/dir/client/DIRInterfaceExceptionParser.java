/*  Copyright (c) 2010 Konrad-Zuse-Zentrum fuer Informationstechnik Berlin.

    This file is part of XtreemFS. XtreemFS is part of XtreemOS, a Linux-based
    Grid Operating System, see <http://www.xtreemos.eu> for more details.
    The XtreemOS project has been developed with the financial support of the
    European Commission's IST program under contract #FP6-033576.

    XtreemFS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 2 of the License, or (at your option)
    any later version.

    XtreemFS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with XtreemFS. If not, see <http://www.gnu.org/licenses/>.
*/
/*
 * AUTHORS: Björn Kolbeck (ZIB)
 */

package org.xtreemfs.dir.client;

import java.io.IOException;

import org.xtreemfs.foundation.oncrpc.client.RemoteExceptionParser;
import org.xtreemfs.foundation.oncrpc.utils.ONCRPCException;
import org.xtreemfs.foundation.oncrpc.utils.XDRUnmarshaller;
import org.xtreemfs.interfaces.DIRInterface.DIRInterface;

/**
 *
 * @author bjko
 */
public class DIRInterfaceExceptionParser extends RemoteExceptionParser {

    public DIRInterfaceExceptionParser() {

    }

    @Override
    public boolean canParseException(int accept_stat) {
        return (accept_stat >= DIRInterface.getVersion() && (accept_stat < DIRInterface.getVersion()+100));
    }

    @Override
    public ONCRPCException parseException(int accept_stat, XDRUnmarshaller unmarshaller) throws IOException {
        try {
            ONCRPCException ex = DIRInterface.createException(accept_stat);
            ex.unmarshal(unmarshaller);
            return ex;
        } catch (IOException ex) {
            throw ex;
        } catch (Exception ex) {
            throw new IOException(ex.getMessage());
        }
    }

}
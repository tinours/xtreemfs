package org.xtreemfs.interfaces.OSDInterface;

import java.io.StringWriter;
import org.xtreemfs.*;
import org.xtreemfs.common.buffer.ReusableBuffer;
import org.xtreemfs.interfaces.*;
import org.xtreemfs.interfaces.utils.*;
import yidl.runtime.Marshaller;
import yidl.runtime.PrettyPrinter;
import yidl.runtime.Struct;
import yidl.runtime.Unmarshaller;




public class xtreemfs_shutdownResponse extends org.xtreemfs.interfaces.utils.Response
{
    public static final int TAG = 2010031286;

    public xtreemfs_shutdownResponse() {  }

    // java.lang.Object
    public String toString()
    {
        StringWriter string_writer = new StringWriter();
        string_writer.append(this.getClass().getCanonicalName());
        string_writer.append(" ");
        PrettyPrinter pretty_printer = new PrettyPrinter( string_writer );
        pretty_printer.writeStruct( "", this );
        return string_writer.toString();
    }

    // java.io.Serializable
    public static final long serialVersionUID = 2010031286;

    // yidl.runtime.Object
    public int getTag() { return 2010031286; }
    public String getTypeName() { return "org::xtreemfs::interfaces::OSDInterface::xtreemfs_shutdownResponse"; }

    public int getXDRSize()
    {
        int my_size = 0;

        return my_size;
    }

    public void marshal( Marshaller marshaller )
    {

    }

    public void unmarshal( Unmarshaller unmarshaller )
    {

    }
}

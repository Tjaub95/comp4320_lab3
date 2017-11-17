import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Server {
    private static final int SERVER_GID = 12;
    private static final int MAX_BUFF_LEN = 1000;
    private static final int MAGIC_NUMBER = 0x4A6F7921;

    public static void main(String[] args) throws IOException {

        int serverPort;

        if (args.length < 1) {
            System.err.println("Parameters required: serverPort");
            System.exit(1);
        }

        try {
            serverPort = Integer.parseInt(args[0]);
        } catch (Exception e) {
            System.err.println("Parameters required: serverPort (must be an integer)");
            System.exit(1);
        }

        serverPort = Integer.parseInt(args[0]);

        // Create a new dgram
        DatagramSocket packetSocket = new DatagramSocket(serverPort);

        byte[] receivedMessage = new byte[MAX_BUFF_LEN - 7];

        DatagramPacket receivePacket = new DatagramPacket(receivedMessage, receivedMessage.length);
        System.out.println("Waiting for client request...");

        boolean clientWaiting = false;
        InetAddress clientAddress = null;
        short clientPort = 0;

        boolean isError = false;

        while (true) {
            packetSocket.receive(receivePacket);
            System.out.println("Packet received...");

            byte[] response = receivePacket.getData();
            int respLen = receivePacket.getLength();

            int received_magic_number = bytesToInt(response, 0);

            byte[] bytesToSend = null;
            short received_port = 0;

            // Check for error in packet length
            if (respLen != 7) {
                bytesToSend = setErrorBytes(2);
                System.out.println("Invalid packet length");
                isError = true;
            }
            // Check for bad magic number
            else if (received_magic_number != MAGIC_NUMBER) {
                bytesToSend = setErrorBytes(1);
                System.out.println("Invalid magic number");
                isError = true;
            } else {
                received_port = bytesToShort(response[4], response[5]);
                byte received_gid = response[6];
                // Check for bad port number
                if (received_port < (received_gid * 5) + 10010 || received_port > (received_gid * 5) + 4 + 10010) {
                    bytesToSend = setErrorBytes(4);
                    System.out.println("Port number out of range");
                    isError = true;
                }
            }

            if (!isError) {
                if (!clientWaiting) {
                    clientWaiting = true;
                    clientAddress = receivePacket.getAddress();
                    clientPort = received_port;

                    bytesToSend = new byte[7];
                    // Magic Number in Network Order
                    System.arraycopy(intToByte(MAGIC_NUMBER), 0, bytesToSend, 0, 4);
                    bytesToSend[4] = (byte) SERVER_GID;
                    System.arraycopy(shortToByte(clientPort), 0, bytesToSend, 5, 2);

                    System.out.println("Client waiting");
                } else {
                    clientWaiting = false;

                    bytesToSend = new byte[11];
                    // Magic Number in Network Order
                    System.arraycopy(intToByte(MAGIC_NUMBER), 0, bytesToSend, 0, 4);
                    bytesToSend[4] = clientAddress.getAddress()[0];
                    bytesToSend[5] = clientAddress.getAddress()[1];
                    bytesToSend[6] = clientAddress.getAddress()[2];
                    bytesToSend[7] = clientAddress.getAddress()[3];
                    System.arraycopy(shortToByte(clientPort), 0, bytesToSend, 8, 2);
                    bytesToSend[10] = (byte) SERVER_GID;

                    System.out.println("Clients matched");
                }
            } else {
                isError = false;
            }

            //Send result packet
            DatagramPacket newPacket = new DatagramPacket(bytesToSend, bytesToSend.length, receivePacket.getAddress(), receivePacket.getPort());
            packetSocket.send(newPacket); // Send new packet back to client

            receivedMessage = new byte[MAX_BUFF_LEN - 7];
            receivePacket = new DatagramPacket(receivedMessage, receivedMessage.length);
        }
    }

    private static byte[] setErrorBytes(int errorCode) {
        byte[] byteToSend = new byte[9];
        // Magic Number in Network Order
        System.arraycopy(intToByte(MAGIC_NUMBER), 0, byteToSend, 0, 4);
        byteToSend[4] = (byte) SERVER_GID;
        System.arraycopy(intToByte(errorCode), 0, byteToSend, 5, 9);
        return byteToSend;
    }

    private static byte[] intToByte(int intToConv) {
        return ByteBuffer.allocate(4).order(ByteOrder.BIG_ENDIAN).putInt(intToConv).array();
    }

    private static byte[] shortToByte(short shortToConv) {
        return ByteBuffer.allocate(2).order(ByteOrder.BIG_ENDIAN).putShort(shortToConv).array();
    }

    private static int bytesToInt(byte[] bytes, int offset) {
        int ret = 0;
        for (int i = 0; i < 4 && i + offset < bytes.length; i++) {
            ret <<= 8;
            ret |= (int) bytes[i] & 0xFF;
        }

        return ret;
    }

    private static short bytesToShort(byte firstHalfOfShort, byte secondHalfOfShort) {
        return (short) ((firstHalfOfShort << 8) | secondHalfOfShort);
    }
}

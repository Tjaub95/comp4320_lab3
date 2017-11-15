import java.io.IOException;
import java.net.DatagramSocket;
import java.net.DatagramPacket;

public class UDPServer {
    private static final int SERVER_GID = 12;
    private static final int MAX_BUFF_LEN = 1000;

    public static void main(String[] args) throws IOException {

        if (args.length < 1) {
            System.err.println("Parameters required: serverPort");
            System.exit(1);
        }

        while (true) {
            // Create a new dgram
            DatagramSocket packetSocket = new DatagramSocket();

            byte[] receivedMessage = new byte[MAX_BUFF_LEN - 9];

            DatagramPacket receivePacket = new DatagramPacket(receivedMessage, receivedMessage.length);
            System.out.println("Waiting for client request...");
            packetSocket.receive(receivePacket);
            System.out.println("Packet received...");

            byte[] response = receivePacket.getData();
            int respLen = receivePacket.getLength();


        }
    }
}

import java.io.IOException;
import java.net.DatagramSocket;
import java.net.DatagramPacket;

public class Server {
    private static final int SERVER_GID = 12;
    private static final int MAX_BUFF_LEN = 1000;

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


        while (true) {
            packetSocket.receive(receivePacket);
            System.out.println("Packet received...");

            byte[] response = receivePacket.getData();
            int respLen = receivePacket.getData().length;
        }
    }
}

# SETUP GUIDE

## Getting Loki Running
This guide provides detailed instructions for setting up and running the Loki application on different operating systems and hardware platforms.

---

## Windows Setup  
1. **Download the Loki Repository**  
   - Clone the repository from GitHub:  
     ```bash  
     git clone https://github.com/Fomorianshifter/Loki.git  
     ```  
2. **Install ARM Compiler**  
   - Download the ARM compiler for Windows:  
   - Follow the installation wizard to complete the installation.
3. **Build the Application**  
   - Navigate to the Loki directory:  
     ```bash  
     cd Loki  
     ```  
   - Run the build command:  
     ```bash  
     make  
     ```  
4. **Run the Application**  
   - Execute the application:  
     ```bash  
     ./loki  
     ```  

---

## Linux Setup  
1. **Download the Loki Repository**  
   - Clone the repository from GitHub:  
     ```bash  
     git clone https://github.com/Fomorianshifter/Loki.git  
     ```  
2. **Install ARM Compiler**  
   - Use your package manager to install the ARM compiler:
     ```bash  
     sudo apt install gcc-arm-linux-gnueabi  
     ```  
3. **Build the Application**  
   - Navigate to the Loki directory:  
     ```bash  
     cd Loki  
     ```  
   - Run the build command:  
     ```bash  
     make  
     ```  
4. **Run the Application**  
   - Execute the application:  
     ```bash  
     ./loki  
     ```  

---

## Mac Setup  
1. **Download the Loki Repository**  
   - Clone the repository from GitHub:  
     ```bash  
     git clone https://github.com/Fomorianshifter/Loki.git  
     ```  
2. **Install ARM Compiler**  
   - Install Arm Development Studio or Xcode with ARM support.
3. **Build the Application**  
   - Navigate to the Loki directory:  
     ```bash  
     cd Loki  
     ```  
   - Run the build command:  
     ```bash  
     make  
     ```  
4. **Run the Application**  
   - Execute the application:  
     ```bash  
     ./loki  
     ```  

---

## Orange Pi Configuration  
1. **Connect the Orange Pi**  
   - Physically connect your Orange Pi to the network and power it on.  
2. **Access the Orange Pi via SSH**  
   - Open a terminal and run:  
     ```bash  
     ssh orangepi@<Orange_Pi_IP_Address>  
     ```  
3. **Deploy the Application**  
   - Transfer the built application to the Orange Pi:
     ```bash  
     scp ./loki orangepi@<Orange_Pi_IP_Address>:/path/to/deploy/  
     ```  

---

## Troubleshooting

### Compiler Not Found  
- Ensure you have installed the compiler correctly.  
- Verify your environment variables are set.  

### Build Failures  
- Check for any error messages during the build process and address them.  
- Ensure all dependencies are installed.

### SSH Connection Issues  
- Confirm the Orange Pi is powered on and connected to the network.  
- Check the IP address and your SSH configuration.

### Hardware Detection Problems  
- Ensure all hardware is properly connected.  
- Check hardware compatibility with the Loki application.

---

This guide should help get your environment set up and troubleshoot common issues. Happy coding!
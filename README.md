Follow the below-mentioned steps to enable sensors in android guest in CiV: <br>

Step 1:  Run iiod daemon in Ubuntu Host OS. <br>
  *  Check if iiod daemon running in host <br>
     - $ps -ef | grep -i iiod <br>

  *  If not running install the iiod daemon. <br>
     - $sudo apt install iiod <br>
     - sudo ./iiod & <br>

Step 2: Launch android in CIV and set ip address property. <br>
  *  $adb root <br>
  *  $adb shell <br>
  *  #setprop ipaddr <Host_ip_addr> <br>

Step 3: Install any third-party sensor android apk in CIV <br>
  *  Verify the sensors list in App. <br>
     E.g. Test Apk "Sensor_Test.apk".

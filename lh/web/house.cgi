#!/bin/bash
echo "Content-type: text/html; charset=UTF-8"
echo "Refresh: 5"
echo

intemp=`../get 100EE90700000047`
outtemp=`../get 10E4790600000099`

entranceLock=closed;../get 0596B11600000079 || entranceLock=open
garageLock=closed;../get 057CAC160000000F || garageLock=open
officeLock=closed;../get 05F3A70200000087 || officeLock=open
postBoxImg=;[[ `cat ../post-in-postbox` = "1" ]] && postBoxImg='<img src="postbox.jpeg" style="position:absolute; left:0; top:150;"/>'
heaterImg=;../get 0549A40200000001 && heaterImg='<img src="Icon_LightningBolt.gif" style="position:absolute; left:135; top:30;"/>';

cat <<EOF
<html>
  <head>
    <title>Hus-status</title>
    <style></style>
  </head>
  <body>
    <div style="position:absolute; left:180; top:20;">
      <img src="house.png"/>
      <img src="lock-$garageLock.png" style="position:absolute; left:30; top:75;"/>
      <img src="lock-$officeLock.png" style="position:absolute; left:5; top:125;"/>
      <img src="lock-$entranceLock.png" style="position:absolute; left:135; top:100;"/>
      <span style="font-size:150%; position:absolute; left:60; top:182;">$intemp&deg;C</span>
      <span style="font-size:150%; position:absolute; left:60; top:214;">$outtemp&deg;C</span>
    </div>
    <img src="volvo_v70.gif" style="position:absolute; left:0; top:10;"/>
    $heaterImg
    $postBoxImg
  </body>
</html>
EOF

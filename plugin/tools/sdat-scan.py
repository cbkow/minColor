#!/usr/bin/env python3
"""Scan an .aep for minColor sequence data (MincSeqData v2, 544 bytes, magic ACnM) and print the
passport fields: configBase @216, passportWorking @344. 212-byte aRbp hits are the v1 arb PARAM.
  python plugin/tools/sdat-scan.py project.aep"""
import sys, struct
d = open(sys.argv[1], 'rb').read()
i = 0; n = 0
while True:
    i = d.find(b'ACnM', i)
    if i < 0: break
    tag = d[i-8:i-4]; size = struct.unpack('>I', d[i-4:i])[0]      # RIFX chunk header precedes the magic
    sd = d[i:i+size]
    n += 1
    print('hit %d @%d tag=%s size=%d' % (n, i, tag.decode('latin1'), size))
    if size == 544:
        magic, ver, dr, iid = struct.unpack('<IHHI', sd[:12])
        space = sd[12:212].split(b'\0')[0].decode()
        seqv, res = struct.unpack('<HH', sd[212:216])
        cfg = sd[216:344].split(b'\0')[0].decode(); pw = sd[344:544].split(b'\0')[0].decode()
        print('   arb v%d dir=%d id=%u space=%r | seqVersion=%d configBase@216=%r passportWorking@344=%r' % (ver, dr, iid, space, seqv, cfg, pw))
    i += 4
print('total hits', n)

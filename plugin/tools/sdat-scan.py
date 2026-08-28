#!/usr/bin/env python3
"""Scan an .aep for minColor sequence data (MincSeqData v2, 544 bytes; magic ACnM) and print the
passport fields: configBase @216, passportWorking @344. 212-byte aRbp hits are the v1 arb PARAM.
  python plugin/tools/sdat-scan.py project.aep"""
i = 0; n = 0
while True:
    i = d.find(b'ACnM', i)
    if i < 0: break
    # sequence data: chunk header precedes the magic: tag(4) + size(4)
    tag = d[i-8:i-4]; size = struct.unpack('>I', d[i-4:i])[0]
    sd = d[i:i+size]
    n += 1
    print('hit %d @%d tag=%s size=%d' % (n, i, tag, size))
    if size == 544:
        magic, ver, dr, iid = struct.unpack('<IHHI', sd[:12])
        space = sd[12:212].split(b'\0')[0].decode()
        seqv, res = struct.unpack('<HH', sd[212:216])
        cfg = sd[216:344].split(b'\0')[0].decode(); pw = sd[344:544].split(b'\0')[0].decode()
        print('   arb v%d dir=%d id=%u space=%r | seqVersion=%d configBase@216=%r passportWorking@344=%r' % (ver, dr, iid, space, seqv, cfg, pw))
    else:
        print('   (not 544; first bytes %s)' % sd[:24])
    i += 4
print('total hits', n)

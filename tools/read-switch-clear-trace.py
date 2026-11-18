"""Decode the version-1 bounded GPU-thread causal trace. No GPU timing claims."""
from pathlib import Path
import argparse
import json
import struct

KINDS = dict(enumerate(('operation','registers','shader','constants','vertex_index',
    'vertex_export','extent','height','ownership','previous_owner','transfer',
    'guest_draw','resolve','shader_code','dump','transfer_rectangle','texture_bindings','resolve_info'),1))

def read_trace(path):
    raw=Path(path).read_bytes()
    if len(raw)<32 or len(raw)%4:
        raise ValueError('Truncated trace header or unaligned payload')
    values=struct.unpack(f'<{len(raw)//4}I',raw)
    magic,version,lo,hi,operations,count,dropped,length=values[:8]
    if magic!=0x5443324c or version!=1 or len(values)!=8+length:
        raise ValueError('Unsupported format or incomplete file')
    records=[]; at=8; operation=0; clear_ops=set(); transfer_draws=0
    while at<len(values):
        if at+3>len(values): raise ValueError('Truncated record header')
        kind,size,op=values[at:at+3]; at+=3
        if kind not in KINDS or at+size>len(values): raise ValueError('Invalid record type or length')
        data=list(values[at:at+size]); at+=size
        if kind==1:
            if size or op!=operation+1: raise ValueError('Nonconsecutive operation ID')
            operation=op
        elif not operation or op!=operation:
            raise ValueError('Record attributed to the wrong operation')
        record={'kind':KINDS[kind],'operation':op,'words':data}
        if kind==2:
            if size%2: raise ValueError('Invalid register delta')
            record['registers']={f'{data[i]:04X}':f'{data[i+1]:08X}' for i in range(0,size,2)}
        elif kind==3:
            if size!=4: raise ValueError('Invalid shader metadata')
            record['stage']='vs' if data[0]==0 else 'ps'
            record['hash']=f'{data[1] | (data[2]<<32):016X}'
            if data[0]==0 and record['hash']=='1E6883FCCDE1F688': clear_ops.add(op)
        elif kind==6:
            if size!=6: raise ValueError('Invalid vertex export')
            record['float_values']=list(struct.unpack('<4f',struct.pack('<4I',*data[2:])))
        elif kind==11:
            transfer_draws+=1
        records.append(record)
    if len(records)!=count or operation>operations or (not dropped and operation!=operations):
        raise ValueError('Header counts do not match complete records')
    return {'frame':lo | (hi<<32),'operations':operations,'records':count,'dropped':dropped,
            'complete':dropped==0,'bytes':len(raw),'clear_operations':sorted(clear_ops),
            'transfer_draws':transfer_draws,'events':records}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace',type=Path)
    parser.add_argument('--output',type=Path,help='Write full ordered JSON evidence')
    args=parser.parse_args()
    result=read_trace(args.trace)
    if args.output: args.output.write_text(json.dumps(result,indent=2,allow_nan=True)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='events'},indent=2))

if __name__=='__main__': main()

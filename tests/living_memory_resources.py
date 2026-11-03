"""RAM-only conversion/render fixtures: no original game assets written to disk."""
import io

class MemoryFile:
    def __init__(self,owner,name):self.owner,self.name=owner,name
    def open(self,mode='rb',*args,**kwargs):
        parent=self
        class Buffer(io.BytesIO):
            def close(self):
                if 'w' in mode:parent.owner.files[parent.name]=self.getvalue()
                super().close()
        return Buffer(b'' if 'w' in mode else self.read_bytes())
    def read_bytes(self):return self.owner.files[self.name]
    def write_bytes(self,value):self.owner.files[self.name]=value;return len(value)
    def write(self,value):self.owner.files[self.name]=self.owner.files.get(self.name,b'')+value;return len(value)
    def read_text(self,*a,**k):return self.read_bytes().decode()
    def write_text(self,value,*a,**k):return self.write_bytes(value.encode())
    def is_file(self):return self.name in self.owner.files
    def unlink(self,*a,**k):self.owner.files.pop(self.name,None)
    def flush(self):pass
    def tell(self):return len(self.owner.files.get(self.name,b''))
    def __str__(self):return self.name

class MemoryDirectory:
    def __init__(self):self.files={}
    def __truediv__(self,name):return MemoryFile(self,str(name))
    def mkdir(self,*a,**k):pass

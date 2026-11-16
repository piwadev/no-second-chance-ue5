# Download every file of a free itch.io asset page (no account needed for pay-what-you-want/free pages).
import sys,re,json,urllib.request,urllib.parse,http.cookiejar,html,os
cj=http.cookiejar.CookieJar(); op=urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cj))
op.addheaders=[('User-Agent','Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/120 Safari/537.36')]
def get(u): return op.open(u,timeout=60).read().decode('utf8','replace')
def post(u,d): return op.open(urllib.request.Request(u,data=urllib.parse.urlencode(d).encode()),timeout=60).read().decode()
page=sys.argv[1].rstrip('/'); out=sys.argv[2] if len(sys.argv)>2 else '.'; flt=sys.argv[3] if len(sys.argv)>3 else ''
h=get(page); tok=re.search(r'name="csrf_token" value="([^"]+)"',h).group(1)
dl=json.loads(post(page+'/download_url',{'csrf_token':tok}))['url']; h2=get(dl)
ups=re.findall(r'data-upload_id="(\d+)"',h2); names=[html.unescape(x) for x in re.findall(r'class="upload_name"[^>]*>.*?title="([^"]+)"',h2,re.S)]
for u,n in zip(ups,names):
    if flt and flt not in n: continue
    r=json.loads(post(f'{page}/file/{u}?source=view_game&as_props=1&after_download_lightbox=1',{'csrf_token':tok}))
    if 'url' not in r: print('ERR',n,str(r)[:200]); continue
    f=os.path.join(out,n); data=op.open(r['url'],timeout=900).read(); open(f,'wb').write(data); print(f,len(data),flush=True)

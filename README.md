## pyfastblur
Small Python library with a single purpose to apply fast blur to PNG images (libpng backend)

# Install
Windows: `python -m pip install pyfastblur`  
Linux:
```
git clone https://github.com/sertraline/pyfastblur && cd pyfastblur
python3 setup.py build bdist_wheel
cd dist && python3 -m pip install ./pyfastblur*.whl
```

### Usage
```python
# example 1
import pyfastblur
result = pyfastblur.blur("path/to/file.png", radius=24)
```
```python
# example 2
result = pyfastblur.blur("path/to/file.png",
                         radius=24,
                         stronger_blur=True)
# stronger_blur makes additional blur passes, is much slower
```
```python
# example 3
from io import BytesIO
# read image into memory object
obj = BytesIO()
with open("test.png", 'rb') as f:
    obj.write(f.read())
# rewind
obj.seek(0)
result = pyfastblur.blur(obj, radius=24)
```
```python
# write result to file
with open("output.png", 'wb') as f:
    f.write(result.read())
```

### Speed
Sample image: [link](https://i.imgur.com/YoR5u6X.png) (3.55MB)  

Code:
```python
import pyfastblur
import time
from io import BytesIO

runs = 6
average = 0.0
for i in range(runs):
    start = time.time()
    # read image into memory object
    obj = BytesIO()
    with open("test.png", 'rb') as f:
        obj.write(f.read())
    # rewind
    obj.seek(0)
    # output: BytesIO object
    out = pyfastblur.blur(obj, radius=64)
    # write to file
    with open("output.png", 'wb') as f:
        f.write(out.read())
    average += (time.time() - start)
average = average / runs

print("Average runtime: %s seconds" % str(average))
```

Result (Windows): `Average runtime: 0.5009152094523112 seconds`


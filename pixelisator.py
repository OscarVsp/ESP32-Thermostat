import sys
from PIL import Image, ImageOps



if __name__ == "__main__":
    try:
        filename = sys.argv[sys.argv.index("-f") +1]
    except (ValueError, IndexError):
        print('Argument "Filename" is required using "-f FILENAME"')
        exit(1)
    #TODO error for unsuported file type
        
    try:
        size_str = sys.argv[sys.argv.index("-s") +1].split(',')
        try:
            size = (int(size_str[0]),int(size_str[1]))
            if size[0] <= 0 or size[1] <= 0:
                raise ValueError
        except (ValueError, IndexError):
            print('Argument "Size" must be a tuple(int,int) of positif integer')
            exit(1)
    except (ValueError, IndexError):
        print('Argument "Size" is required using "-s SIZE"')
        exit(1)
    
        
    try:
        rotate_str = sys.argv[sys.argv.index("-r") +1]
        try:
            rotate = int(rotate_str)
            if rotate < 0 or rotate > 3:
                raise ValueError
        except ValueError:
            print('Argument "Rotation" must be {0, 1, 2, 3}')
            exit(1)
    except IndexError:
        print('Argument "Rotation" not found after "-r"')
        exit(1)
    except ValueError:
        rotate = 3
        
    print(f"Image: {filename}\nSize: {size[0]}x{size[1]}\nRotation: {90*rotate}°")
    
    name = filename.split('/')[-1].split('.')[:-1][0]
        
        
    with Image.open(filename) as img:
        img = ImageOps.invert(img).convert("1").rotate(90*rotate).resize(size, Image.ANTIALIAS)
        img_bt = img.tobitmap(name=name)
        img_bt = str(img_bt)[2:-1].split('\\n')
        n_bytes_lines = len(img_bt)-4
        defines = img_bt[0]+"\n"+img_bt[1]
        bytes_str = "".join(img_bt[3:-1]).split(',')
        n_lines = (len(bytes_str)-1)//16 + 1
        bitmap_lines = []
        for n in range(n_lines):
            bitmap_lines.append(", ".join(bytes_str[n*16:(n+1)*16 if (n+1)*16 < len(bytes_str) else len(bytes_str)]))
        print("const unsigned char "+name+"[] PROGMEM = {\n  "+',\n  '.join(bitmap_lines)+"\n};")
        

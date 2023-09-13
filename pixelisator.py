import sys
from PIL import Image

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
        rotate = 1
        
    print(f"Image: {filename}\nSize: {size[0]}x{size[1]}\nRotation: {90*rotate}°")
    
    name = filename.split('/')[-1].split('.')[:-1][0]
        
        
    with Image.open(filename) as img:
        img_BW = img.convert("1")
        img_rot = img_BW.rotate(90*rotate)
        img_rez = img_rot.resize(size, Image.ANTIALIAS)
        bitmap = img_rez.tobitmap(name=name)
        print(bitmap)
        """for i in range(len(bitmap)):
            print(hex(bitmap[i]), end=", ")"""
        
#!/usr/bin/env python3
"""
convert_weights.py
Convert INT4 quantized PyTorch model to binary format for C++ firmware

Usage:
    python convert_weights.py model.pt output.bin
"""

import sys
import struct
import torch
import numpy as np

def quantize_to_int4(tensor, scale=None, zero_point=0):
    """
    Quantize tensor to INT4 format
    Returns: (quantized_data, scale, zero_point)
    """
    if scale is None:
        max_val = tensor.abs().max().item()
        scale = max_val / 7.0  
        if scale == 0:
            scale = 1.0
    
    quantized = torch.clamp(
        torch.round(tensor / scale + zero_point),
        -8, 7
    ).to(torch.int8)
    
    return quantized.numpy(), scale, zero_point

def pack_int4_weights(weights):
    if len(weights) % 2 != 0:
        weights = np.append(weights, 0)
    
    packed = []
    for i in range(0, len(weights), 2):
        w0 = int(weights[i]) & 0x0F      # Lower 4 bits
        w1 = int(weights[i+1]) & 0x0F    # Upper 4 bits
        byte = (w1 << 4) | w0
        packed.append(byte)
    
    return bytes(packed)

def convert_model(pt_file, bin_file):
    print(f"Loading PyTorch model from: {pt_file}")
    
    try:
        # Load checkpoint
        checkpoint = torch.load(pt_file, map_location='cpu')
        
        # Extract model state dict
        if isinstance(checkpoint, dict):
            if 'model' in checkpoint:
                state_dict = checkpoint['model']
            elif 'state_dict' in checkpoint:
                state_dict = checkpoint['state_dict']
            else:
                state_dict = checkpoint
        else:
            state_dict = checkpoint.state_dict()
        
        print(f"Model has {len(state_dict)} parameters")
        
        # Infer model config from shapes
        config = infer_config(state_dict)
        
        print(f"\nModel configuration:")
        print(f"  Layers: {config['num_layers']}")
        print(f"  Hidden size: {config['hidden_size']}")
        print(f"  Num heads: {config['num_heads']}")
        print(f"  Vocab size: {config['vocab_size']}")
        print(f"  Max seq len: {config['max_seq_len']}")
        print(f"\nWriting binary to: {bin_file}")
        
        checksum_data = []
        
        with open(bin_file, 'wb') as f:
            header = struct.pack(
                'IIIIIIII',
                0x57544E54,  # Magic: "WTNT"
                1,           # Version
                config['num_layers'],
                config['hidden_size'],
                config['num_heads'],
                config['vocab_size'],
                config['max_seq_len'],
                config['intermediate_size']
            )
            f.write(header)
            
            checksum_offset = f.tell()
            f.write(struct.pack('I', 0))  # Will update later
            
            print("\nWriting embeddings...")
            embed_key = find_key(state_dict, ['token_embeddings', 'embeddings.word_embeddings.weight', 'wte.weight'])
            if embed_key:
                embed = state_dict[embed_key]
                print(f"  Token embeddings: {embed.shape}")
                
                embed_fp16 = embed.half().numpy().tobytes()
                f.write(embed_fp16)
                checksum_data.append(('embeddings', embed_fp16))
            else:
                print("  Warning: No token embeddings found")
                embed_size = config['vocab_size'] * config['hidden_size']
                zeros = np.zeros(embed_size, dtype=np.float16).tobytes()
                f.write(zeros)
                checksum_data.append(('embeddings', zeros))
            
            pos_key = find_key(state_dict, ['position_embeddings', 'embeddings.position_embeddings.weight', 'wpe.weight'])
            if pos_key:
                pos = state_dict[pos_key]
                pos_fp16 = pos.half().numpy().tobytes()
                f.write(pos_fp16)
                checksum_data.append(('pos_embeddings', pos_fp16))
            else:
                pos_size = config['max_seq_len'] * config['hidden_size']
                zeros = np.zeros(pos_size, dtype=np.float16).tobytes()
                f.write(zeros)
                checksum_data.append(('pos_embeddings', zeros))
        
            for layer_idx in range(config['num_layers']):
                print(f"\nProcessing layer {layer_idx}...")

                layer_weights = extract_layer_weights(state_dict, layer_idx)

                for name, tensor in layer_weights.items():
                    if tensor is not None:
                        quantized, scale, zp = quantize_to_int4(tensor)
                        packed = pack_int4_weights(quantized.flatten())
                        
                        metadata = struct.pack('fbi', scale, zp, len(packed))
                        f.write(metadata)
                        a
                        f.write(packed)
                        
                        checksum_data.append((f'layer_{layer_idx}_{name}', packed))
                        
                        print(f"  {name}: {tensor.shape} -> {len(packed)} bytes (INT4, scale={scale:.6f})")
                    else:
                        print(f"  {name}: NOT FOUND (writing zeros)")
                        size = config['hidden_size'] * config['hidden_size'] // 2
                        metadata = struct.pack('fbi', 1.0, 0, size)
                        f.write(metadata)
                        zeros = bytes(size)
                        f.write(zeros)
                        checksum_data.append((f'layer_{layer_idx}_{name}', zeros))

            print("\nCalculating checksums...")
            import hashlib
            
            checksums = []
            for name, data in checksum_data:
                checksum = hashlib.sha256(data).digest()
                checksums.append((name, checksum))
                print(f"  {name}: {checksum[:8].hex()}...")
            
            checksum_section_offset = f.tell()
            f.write(struct.pack('I', len(checksums)))
            for name, checksum in checksums:
                name_bytes = name.encode('utf-8')
                f.write(struct.pack('I', len(name_bytes)))
                f.write(name_bytes)
                f.write(checksum)
            
            f.seek(checksum_offset)
            f.write(struct.pack('I', checksum_section_offset))
            
            print(f"\nConversion complete!")
            print(f"Output file: {bin_file}")
            print(f"File size: {checksum_section_offset / (1024*1024):.2f} MB")
            
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    return True

def infer_config(state_dict):
    """
    Infer model configuration from state dict
    """
    config = {
        'num_layers': 0,
        'hidden_size': 0,
        'num_heads': 0,
        'vocab_size': 0,
        'max_seq_len': 2048,
        'intermediate_size': 0
    }
    
    layer_keys = [k for k in state_dict.keys() if 'layer' in k.lower() or 'block' in k.lower()]
    if layer_keys:
        import re
        layer_nums = []
        for key in layer_keys:
            match = re.search(r'(\d+)', key)
            if match:
                layer_nums.append(int(match.group(1)))
        if layer_nums:
            config['num_layers'] = max(layer_nums) + 1
    
    for key, tensor in state_dict.items():
        if 'weight' in key and len(tensor.shape) >= 2:
            config['hidden_size'] = tensor.shape[-1]
            break
    
    embed_key = find_key(state_dict, ['token_embeddings', 'embeddings.word_embeddings.weight', 'wte.weight'])
    if embed_key:
        config['vocab_size'] = state_dict[embed_key].shape[0]
    
    config['intermediate_size'] = config['hidden_size'] * 4
    
    config['num_heads'] = max(1, config['hidden_size'] // 64)
    
    return config

def find_key(state_dict, possible_keys):
    """Find first matching key from list"""
    for key in possible_keys:
        if key in state_dict:
            return key
    return None

def extract_layer_weights(state_dict, layer_idx):
    """
    Extract weights for a specific layer
    """
    weights = {
        'q_weights': None,
        'k_weights': None,
        'v_weights': None,
        'o_weights': None,
        'ffn_up': None,
        'ffn_down': None,
    }
    
    patterns = {
        'q_weights': [f'layers.{layer_idx}.attention.query.weight',
                      f'blocks.{layer_idx}.attn.q_proj.weight',
                      f'transformer.h.{layer_idx}.attn.c_attn.weight'],
        'k_weights': [f'layers.{layer_idx}.attention.key.weight',
                      f'blocks.{layer_idx}.attn.k_proj.weight'],
        'v_weights': [f'layers.{layer_idx}.attention.value.weight',
                      f'blocks.{layer_idx}.attn.v_proj.weight'],
        'o_weights': [f'layers.{layer_idx}.attention.output.weight',
                      f'blocks.{layer_idx}.attn.o_proj.weight',
                      f'transformer.h.{layer_idx}.attn.c_proj.weight'],
        'ffn_up': [f'layers.{layer_idx}.ffn.up.weight',
                   f'blocks.{layer_idx}.mlp.up_proj.weight',
                   f'transformer.h.{layer_idx}.mlp.c_fc.weight'],
        'ffn_down': [f'layers.{layer_idx}.ffn.down.weight',
                     f'blocks.{layer_idx}.mlp.down_proj.weight',
                     f'transformer.h.{layer_idx}.mlp.c_proj.weight'],
    }
    
    for weight_name, keys in patterns.items():
        for key in keys:
            if key in state_dict:
                weights[weight_name] = state_dict[key]
                break
    
    return weights

def main():
    if len(sys.argv) != 3:
        print("Usage: python convert_weights.py model.pt output.bin")
        sys.exit(1)
    
    pt_file = sys.argv[1]
    bin_file = sys.argv[2]
    
    if not pt_file.endswith('.pt') and not pt_file.endswith('.pth'):
        print("Warning: Input file should be .pt or .pth")
    
    success = convert_model(pt_file, bin_file)
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()

//
//  SMBIOS.swift
//  Kernel
//
//  Created by Adam Kopeć on 10/16/17.
//  Copyright © 2017-2018 Adam Kopeć. All rights reserved.
//

import Addressing
import Loggable
import UUID

struct SMBIOS: Module {
    fileprivate let SMBIOS_Signature: String = "_SM_"
    
    let Name = "SMBIOS"
    
    fileprivate(set) var VersionMajor: UInt8 = 0xFF
    fileprivate(set) var VersionMinor: UInt8 = 0xFF
    fileprivate(set) var EntryCount:    Int  = 0x00
    fileprivate(set) var Length:        Int  = 0x00
    fileprivate var TableAddress: Address = Address(0)
    fileprivate var Tables = [Entry]()
    
    fileprivate(set) var ProductDisplayName: String /*{
        return ConversionDictionary[ProductName] ??*/ = "Generic Device"
    //}
    
    fileprivate(set) var BIOSVendor:         String = "Generic"
    fileprivate(set) var BIOSVersion:        String = "0.0"
    fileprivate(set) var BIOSReleaseDate:    String = "10/16/2001"
    fileprivate(set) var SystemVendor:       String = "Generic"
    fileprivate(set) var ProductName:        String = "Generic1,1"
    fileprivate(set) var ProductVersion:     String = "1.0"
    fileprivate(set) var ProductSerial:      String = "000000000000"
    fileprivate(set) var ProductUUID:        UUID   = UUID.Nil
    fileprivate(set) var BoardVendor:        String = "Generic"
    fileprivate(set) var BoardName:          String = "Generic0,1"
    fileprivate(set) var BoardVersion:       String = "1.0"
    fileprivate(set) var BoardSerial:        String = "000000000000"
    fileprivate(set) var ChassisVendor:      String = "Generic"
    fileprivate(set) var ChassisType:        String = "0"
    fileprivate(set) var ChassisVersion:     String = "1.0"
    fileprivate(set) var ChassisSerial:      String = "000000000000"
    
    var description: String {
        return "\(VersionMajor).\(VersionMinor): \(EntryCount) entries @ \(TableAddress), Length: \(Length)"
    }
    
    init?(structure: SMBIOS_for_Swift) {
        if !structure.foundInBios {
            if (structure.OriginalAddress == 0) || (structure.OriginalAddress == UInt.max) {
                Log("Original Address is not valid", level: .Error)
                return nil
            }
        }
        if String(&structure.SMBIOS.pointee.anchor.0, maxLength: 4) != SMBIOS_Signature {
            Log("Original Address is \(String(structure.OriginalAddress, radix: 16)), Mapped Address is \(String(UInt(bitPattern: structure.SMBIOS), radix: 16))", level: .Error)
            Log("Anchor is \(String(&structure.SMBIOS.pointee.anchor.0, maxLength: 4))", level: .Error)
            return nil
        }
        Length       =     Int(structure.SMBIOS.pointee.table_length)
        EntryCount   =     Int(structure.SMBIOS.pointee.entry_count)
        TableAddress = Address(structure.SMBIOS.pointee.table_address)
        if structure.SMBIOS.pointee.bcd_revision != 0 {
            VersionMajor = structure.SMBIOS.pointee.bcd_revision >> 4
            VersionMinor = structure.SMBIOS.pointee.bcd_revision & 0xF
        } else {
            VersionMajor = structure.SMBIOS.pointee.major_version
            VersionMinor = structure.SMBIOS.pointee.minor_version
        }
        
        Log(description, level: .Debug)
        
        if structure.SMBIOS.pointee.table_address == UInt32.max {
            Log("Table Address is not valid", level: .Error)
            return nil
        }
        Tables.reserveCapacity(EntryCount)
        let buffer = MemoryBuffer(TableAddress.virtual, size: Length)
        for _ in 1 ... EntryCount {
                guard let type:   UInt8  = buffer.read() else { continue }
                guard let length: UInt8  = buffer.read() else { continue }
                guard let handle: UInt16 = buffer.read() else { continue }
                      let Length         = Int(length) - 4
                guard Length >= 0 else {
                    continue
                }
                let data        = buffer.subBufferAtOffset(buffer.offset, size: Length)
                let stringTable = buffer.subBufferAtOffset(buffer.offset + Length)
                guard let terminator: UInt16 = stringTable.read() else { continue }
                var strings = [String]()
                if terminator != 0 {
                    stringTable.offset = 0
                    repeat {
                        guard let string = stringTable.scanASCIIZString() else { break }
                        if string != "" {
                            strings.append(string)
                        } else {
                            break
                        }
                    } while(true)
                }
                Tables.append(Entry(type: type, Length: length, Handle: handle, Data: data, Strings: strings))
                buffer.offset = buffer.offset + Length + stringTable.offset
        }
        
        for entry in Tables {
            switch entry.type {
            case 0:
                BIOSVendor      = entry.getString(from: 4) ?? BIOSVendor
                BIOSVersion     = entry.getString(from: 5) ?? BIOSVersion
                BIOSReleaseDate = entry.getString(from: 8) ?? BIOSReleaseDate
                Log("BIOS Information: " + "\n" + "Vendor: \(BIOSVendor) Version: \(BIOSVersion)" + "\n" + "Date: \(BIOSReleaseDate)", level: .Debug)
            case 1:
                SystemVendor    = entry.getString(from: 4) ?? SystemVendor
                ProductName     = entry.getString(from: 5) ?? ProductName
                ProductVersion  = entry.getString(from: 6) ?? ProductVersion
                ProductSerial   = entry.getString(from: 7) ?? ProductSerial
                ProductUUID     = UUID(asString: entry.getString(from: 8) ?? "") ?? UUID.Nil
                Log("System Information: " + "\n" + "Vendor: \(SystemVendor) Product: \(ProductName) Version: \(ProductVersion)" + "\n" + "Serial: \(ProductSerial) UUID: \(ProductUUID.description)", level: .Debug)
            case 2:
                BoardVendor     = entry.getString(from: 4) ?? BoardVendor
                BoardName       = entry.getString(from: 5) ?? BoardName
                BoardVersion    = entry.getString(from: 6) ?? BoardVersion
                BoardSerial     = entry.getString(from: 7) ?? BoardSerial
                Log("Base board Information: " + "\n" + "Vendor: \(BoardVendor) Name: \(BoardName) Version: \( BoardVersion)" + "\n" + "Serial: \(BoardSerial)", level: .Debug)
            case 3:
                ChassisVendor   = entry.getString(from: 4) ?? ChassisVendor
                ChassisType     = entry.getString(from: 5) ?? ChassisType
                ChassisVersion  = entry.getString(from: 6) ?? ChassisVersion
                ChassisSerial   = entry.getString(from: 7) ?? ChassisSerial
                Log("Chassis Information: " + "\n" + "Venodr: \(ChassisVendor) Type: \(ChassisType) Version: \(ChassisVersion)" + "\n" + "Serial: \(ChassisSerial)", level: .Debug)
            default:
                continue
            }
        }
        var ConversionDictionary = ["VMware7,1" : "VMware Virtual Machine", "MacBookPro14,3" : "MacBook Pro (15-inch, Mid 2017)", "MacBookPro14,2" : "MacBook Pro (13-inch, Mid 2017)", "MacBookPro14,1" : "MacBook Pro (13-inch, Mid 2017)", "MacBookPro13,3" : "MacBook Pro (15-inch, Late 2016)", "MacBookPro13,2" : "MacBook Pro (13-inch, Late 2016)", "MacBookPro13,1" : "MacBook Pro (13-inch, Late 2016)"]
        ProductDisplayName = ConversionDictionary[ProductName] ?? "Generic Device"
    }
    
    fileprivate struct Entry: Loggable, CustomStringConvertible {
        let Name:    String = "SMBIOS"
        let type:    UInt8
        let Length:  UInt8
        let Handle:  UInt16
        let Data:    MemoryBuffer
        let Strings: [String]
        
        var description: String {
            return "Type: \(type), length \(Length) Strings: \(Strings)"
        }
        
        func getString(from offset: UInt8) -> String? {
            guard offset >= 4 else { return nil }
            let Index = Int(offset) - 4
            if let index: UInt8 = Data.readAtIndex(Index) {
                let StringID = Int(index)
                guard StringID > 0 && StringID <= Strings.count else { return nil }
                return Strings[StringID - 1]
            } else {
                Log("Error reading ID", level: .Error)
                return nil
            }
        }
    }
}
